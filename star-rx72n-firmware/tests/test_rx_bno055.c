/* tests/test_rx_bno055.c */

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
 * RIIC channel used by "i2c1"). The real rx_bus_i2c.c and mock bus manager
 * are linked so bus resolution and RIIC calls exercise the full software stack.
 *
 * @par Test Architecture
 *
 * The BNO055 driver uses bus name "i2c1" on RIIC channel 1 (address 0x28).
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
 * | Group         | Tests | Description                                           |
 * |---------------|-------|-------------------------------------------------------|
 * | Init          | 5     | null ptr, success, I2C error, chip ID, double         |
 * | Read          | 5     | null ptr, before-init, euler, quat, I2C error         |
 * | Calibration   | 2     | null ptr, reads status byte                           |
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
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
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
 * address 0x28. The bus name "i2c1" must match the string hardcoded in
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
  k_test_calib_status_raw = 0xFC, /**< Calib status: SYS=3, GYR=3, ACC=3, MAG=0 */
  k_test_wrong_chip_id    = 0xFF, /**< Non-BNO055 chip ID value */
} test_bno055_misc_t;

/**
 * @enum test_bno055_buf_sizes_t
 * @brief Buffer sizes for mock data setup
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_test_single_byte_buf = 1, /**< Single byte read buffer */
  k_test_euler_buf_size  = 6, /**< Euler angles: 3 x int16 */
  k_test_quat_buf_size   = 8, /**< Quaternion: 4 x int16 */
  k_test_lia_buf_size    = 6, /**< Linear accel: 3 x int16 */
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
 * @details Configured in setUp() to match the "i2c1" bus expected by rx_bno055.c.
 * @since Version 1.0.0
 */
static rx_bus_config_t s_i2c_config;

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
 * @post RIIC channel 1 RX buffer contains 0xA0 (chip ID)
 *
 * @since Version 1.0.0
 */
static void internal_load_valid_chip_id(void)
{
  uint8_t chip_id_data = (uint8_t)k_bno055_chip_id_expected;
  mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, &chip_id_data, k_test_single_byte_buf);
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
 * @post RIIC channel 1 RX buffer set for read sequence
 *
 * @since Version 1.0.0
 */
static void internal_load_read_data(void)
{
  uint8_t read_buf[k_read_buf_size];
  memset(read_buf, 0, sizeof(read_buf));

  /* Euler heading = 0x0140, roll = 0x0010, pitch = 0x0020 */
  read_buf[k_read_idx_0] = (uint8_t)k_test_euler_h_lsb;
  read_buf[k_read_idx_1] = (uint8_t)k_test_euler_h_msb;
  read_buf[k_read_idx_2] = (uint8_t)k_test_euler_r_lsb;
  read_buf[k_read_idx_3] = (uint8_t)k_test_euler_r_msb;
  read_buf[k_read_idx_4] = (uint8_t)k_test_euler_p_lsb;
  read_buf[k_read_idx_5] = (uint8_t)k_test_euler_p_msb;

  mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, read_buf, k_read_buf_size);
}

/**
 * @brief Pre-load mock RIIC for quaternion read data
 *
 * @details
 * Loads identity quaternion: W=16384 (0x4000), X=Y=Z=0.
 *
 * @pre mock RIIC channel 1 is initialized
 * @post RIIC channel 1 RX buffer set with quaternion data
 *
 * @since Version 1.0.0
 */
static void internal_load_quat_read_data(void)
{
  uint8_t quat_buf[k_quat_buf_size];
  memset(quat_buf, 0, sizeof(quat_buf));
  quat_buf[k_quat_w_lsb_idx] = (uint8_t)k_test_quat_w_lsb;
  quat_buf[k_quat_w_msb_idx] = (uint8_t)k_test_quat_w_msb;

  mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, quat_buf, k_quat_buf_size);
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
 * 5. Register bus with manager as "i2c1"
 * 6. Initialize RIIC channel 1 via rx_bus_i2c_init()
 *
 * The driver statics s_initialized and s_manager are reset to their
 * initial (uninitialized) state before every test. Tests that need a live
 * driver must call internal_setup_initialized_driver() themselves.
 *
 * @pre None - called by Unity before each test
 * @post s_initialized == false; s_test_manager ready with "i2c1" registered
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
                               "i2c1",
                               (uint8_t)k_test_bno055_riic_ch,
                               (uint8_t)k_test_bno055_i2c_addr,
                               k_rx_p2_0,
                               k_rx_p2_1,
                               (uint32_t)k_test_bno055_freq_hz);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_add_bus(&s_test_manager, &s_i2c_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_i2c_init(&s_test_manager, "i2c1");
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
 * @post s_test_manager deinitialized; mock RIIC state cleared
 *
 * @since Version 1.0.0
 */
void tearDown(void)
{
  (void)rx_bus_manager_deinit(&s_test_manager);
  (void)mock_riic_init();
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
 * @post s_initialized == true; driver ready for read/calib operations
 *
 * @since Version 1.0.0
 */
static void internal_setup_initialized_driver(void)
{
  internal_load_valid_chip_id();
  rx_err_t err = rx_bno055_init(&s_test_manager);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
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
 * @post s_initialized unchanged
 *
 * @since Version 1.0.0
 */
void test_bno055_init_null_manager_returns_error(void)
{
  rx_err_t err = rx_bno055_init(nullptr);
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
 * @post s_initialized == false (error path does not set it)
 *
 * @since Version 1.0.0
 */
void test_bno055_init_i2c_error_propagates(void)
{
  mock_riic_simulate_nack(true);

  rx_err_t err = rx_bno055_init(&s_test_manager);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  mock_riic_simulate_nack(false);
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
 * @post s_initialized == false (chip ID mismatch aborts init)
 *
 * @since Version 1.0.0
 */
void test_bno055_init_wrong_chip_id(void)
{
  uint8_t wrong_id = (uint8_t)k_test_wrong_chip_id;
  mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, &wrong_id, k_test_single_byte_buf);

  rx_err_t err = rx_bno055_init(&s_test_manager);

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
 * @post s_initialized unchanged (still false)
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
 * @post s_initialized == true
 *
 * @since Version 1.0.0
 */
void test_bno055_init_success(void)
{
  internal_load_valid_chip_id();

  rx_err_t err = rx_bno055_init(&s_test_manager);

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
 * @post s_initialized == true (set by first init, unchanged by failed second init)
 *
 * @since Version 1.0.0
 */
void test_bno055_init_double_init_returns_error(void)
{
  /* First init must succeed to set s_initialized = true */
  internal_setup_initialized_driver();

  /* Second call must be rejected with double-init guard */
  rx_err_t err = rx_bno055_init(&s_test_manager);

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
 * @post No side effects
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
 * @post out populated with mocked sensor data
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
 * @post out->quat_w == 0x4000
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
 * @brief rx_bno055_read propagates I2C error that occurs during data read
 *
 * @details
 * Calls internal_setup_initialized_driver() to set s_initialized == true,
 * then injects NACK. The read should fail on the first I2C write-read
 * (Euler registers) and propagate k_rx_err_nack.
 *
 * @pre setUp() has reset s_initialized == false
 * @post No side effects on driver state
 *
 * @since Version 1.0.0
 */
void test_bno055_read_i2c_error_propagates(void)
{
  internal_setup_initialized_driver();
  mock_riic_simulate_nack(true);

  bno055_data_t data;
  rx_err_t      err = rx_bno055_read(&data);

  TEST_ASSERT_EQUAL(k_rx_err_nack, err);
  TEST_ASSERT_NOT_EQUAL(k_rx_ok, err);

  mock_riic_simulate_nack(false);
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
 * @post No side effects
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
 * @post out_calibrated correctly reflects the mocked status byte
 *
 * @since Version 1.0.0
 */
void test_bno055_is_calibrated_reads_status(void)
{
  internal_setup_initialized_driver();

  /* Load fully calibrated status */
  uint8_t calib_full = (uint8_t)k_test_calib_all_full;
  mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, &calib_full, k_test_single_byte_buf);

  bool     out_calibrated = false;
  rx_err_t err            = rx_bno055_is_calibrated(&out_calibrated);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(out_calibrated);

  /* Load uncalibrated status */
  uint8_t calib_none = (uint8_t)k_test_calib_none;
  mock_riic_set_rx_data((uint8_t)k_test_bno055_riic_ch, &calib_none, k_test_single_byte_buf);

  out_calibrated = true;
  err            = rx_bno055_is_calibrated(&out_calibrated);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(out_calibrated);
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
  RUN_TEST(test_bno055_init_i2c_error_propagates);
  RUN_TEST(test_bno055_init_wrong_chip_id);
  RUN_TEST(test_bno055_read_before_init);
  RUN_TEST(test_bno055_init_success);
  RUN_TEST(test_bno055_init_double_init_returns_error);

  /* Read tests - require s_initialized == true */
  RUN_TEST(test_bno055_read_null_ptr_returns_error);
  RUN_TEST(test_bno055_read_success_euler);
  RUN_TEST(test_bno055_read_success_quaternion);
  RUN_TEST(test_bno055_read_i2c_error_propagates);

  /* Calibration tests - require s_initialized == true */
  RUN_TEST(test_bno055_is_calibrated_null_returns_error);
  RUN_TEST(test_bno055_is_calibrated_reads_status);

  return UNITY_END();
}
