/**
 * @file rx_bno055.c
 * @brief BNO055 9-DOF Absolute Orientation Sensor Driver Implementation
 *
 * @details
 * # Overview
 *
 * Complete driver implementation for the Bosch BNO055 sensor connected
 * via I2C on RIIC1 (SCL1=P2.1, SDA1=P2.0). The sensor is initialized in
 * NDOF (Nine Degrees of Freedom) sensor fusion mode which provides:
 * - Absolute Euler angles (heading, roll, pitch)
 * - Unit quaternion for 3D rotation
 * - Gravity-free linear acceleration
 * - On-chip temperature
 * - Per-subsystem calibration status
 *
 * # Module Architecture
 *
 * The driver uses two internal helper functions for all I2C communication:
 * - internal_write_reg(): Writes a single byte to a register
 * - internal_read_regs(): Burst reads N consecutive registers
 *
 * Both helpers use the bus manager abstraction with bus name "i2c1_imu".
 * The device address 0x28 is embedded in the "i2c1_imu" bus configuration
 * registered by main.c.
 *
 * # Data Flow
 *
 * @code{.unparsed}
 * imu_task -----> rx_bno055_read()
 *                      |
 *                 internal_read_regs()
 *                      |
 *                 rx_bus_i2c_write_read()
 *                      |
 *                 RIIC1 HAL
 *                      |
 *                 BNO055 sensor
 * @endcode
 *
 * # NASA Power of 10 Compliance
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. No goto | [PASS] | Structured if/while only |
 * | 2. Bounded loops | [PASS] | No loops in this driver |
 * | 3. No dynamic memory | [PASS] | All buffers on stack |
 * | 4. Short functions | [PASS] | Max function ~50 lines |
 * | 5. Assertions | [PASS] | 2+ checks per function |
 * | 6. Data scope | [PASS] | All locals at point of use |
 * | 7. Check returns | [PASS] | All I2C returns validated |
 * | 8. Limit preprocessor | [PASS] | C23 typed enums only |
 * | 9. Pointer restrictions | [WARN] | bus_manager function pointers (DIP) |
 * | 10. Compile warnings | [PASS] | -Wall -Wextra -Werror |
 *
 * @see rx_bno055.h Public API
 * @see rx_bno055_regs.h Register definitions
 *
 * @author STAR Team
 * @date 2026-03-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_bno055.h"

#include <limits.h>
#include <stddef.h>

#include "rx_bno055_regs.h"
#include "rx_bus_i2c.h"
#include "rx_check.h"
#include "rx_log.h"
#include "tx_api.h"

/* Validate k_bno055_ms_per_tick is consistent with the actual ThreadX tick rate.
 * If TX_TIMER_TICKS_PER_SECOND changes, this assert catches the mismatch at compile time. */
static_assert(
  (bool)(((k_bno055_ms_per_second % TX_TIMER_TICKS_PER_SECOND) == 0U) != 0),
  "TX_TIMER_TICKS_PER_SECOND must evenly divide k_bno055_ms_per_second (no truncation)");
static_assert((bool)((k_bno055_ms_per_tick ==
                      (k_bno055_ms_per_second / TX_TIMER_TICKS_PER_SECOND)) != 0),
              "k_bno055_ms_per_tick must equal k_bno055_ms_per_second/TX_TIMER_TICKS_PER_SECOND");

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum bno055_i2c_size_t
 * @brief I2C write/read buffer size constants for register access
 *
 * @details
 * The BNO055 register write protocol sends [register_address, data_byte]
 * as a 2-byte I2C write transaction. Read commands send a 1-byte address.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_write_buf_size = 2, /**< Size of [reg, val] write buffer */
  k_bno055_read_cmd_size  = 1, /**< Size of register address read command */
} bno055_i2c_size_t;

/**
 * @enum bno055_assert_val_t
 * @brief Compile-time assertion reference values for internal_assemble_int16_le
 *
 * @details
 * Named constants used in static_assert comparisons to avoid magic numbers.
 * k_bno055_expected_msb_shift verifies that k_bno055_shift_msb == 8 (correct
 * for little-endian byte assembly). k_bno055_expected_uint16_bits verifies
 * that uint16_t is exactly 16 bits wide (required for the assembly logic).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_expected_msb_shift   = 8U,  /**< Expected value of k_bno055_shift_msb */
  k_bno055_expected_uint16_bits = 16U, /**< Expected bit width of uint16_t */
} bno055_assert_val_t;

/**
 * @enum bno055_i2c_write_idx_t
 * @brief Byte indices into the 2-byte I2C write buffer
 *
 * @details
 * The write buffer layout is [register_address, data_byte]. These indices
 * name each position to prevent magic-number indexing into the buffer.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_write_idx_reg = 0, /**< Index of register address in write buffer */
  k_bno055_write_idx_val = 1, /**< Index of register value in write buffer */
} bno055_i2c_write_idx_t;

/**
 * @enum bno055_single_byte_size_t
 * @brief Single byte read/write sizes
 *
 * @details
 * Used for reading single registers such as temperature and calib_stat.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_single_byte = 1, /**< Read/write size for single byte operations */
} bno055_single_byte_size_t;

/* =============================================================================
 * Module-Static State
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for this module
 * @details Used as prefix in all rx_log_* calls to identify the BNO055 driver.
 * @note Constant; never modified after initialization
 * @since Version 1.0.0
 */
static const char* const s_tag = "BNO055";

/**
 * @var s_manager
 * @brief Bus manager pointer stored during rx_bno055_init(), used by all I2C operations
 * @details Set to the caller-provided manager in rx_bno055_init(). Set back to NULL
 *          on all failure paths within rx_bno055_init() so a failed init leaves the
 *          module in a safe, re-initializable state.
 * @note NULL before successful init and on any init failure path
 * @warning Must not be accessed directly outside of this module
 * @since Version 1.0.0
 */
static rx_bus_manager_t* s_manager = NULL;

/**
 * @var s_initialized
 * @brief Guard flag: true only after successful rx_bno055_init()
 * @details Set to true at the end of rx_bno055_init() when all init steps succeed,
 *          including the chip ID verification. Remains false on any failure path.
 *          Checked at the top of rx_bno055_read() and rx_bno055_is_calibrated().
 * @invariant s_initialized == true implies s_manager is non-NULL
 * @note Only modified in rx_bno055_init()
 * @since Version 1.0.0
 */
static bool s_initialized = false;

/**
 * @var s_mode
 * @brief Operating mode stored during rx_bno055_init(), used by diagnostic queries
 * @details Set to config->mode during rx_bno055_init(). Reset to k_bno055_mode_poll
 *          by rx_bno055_test_reset_state() (UNIT_TEST builds only).
 * @note Valid only after successful rx_bno055_init()
 * @since Version 1.0.0
 */
static bno055_mode_t s_mode = k_bno055_mode_poll;

/**
 * @var s_bus_name
 * @brief I2C bus name used by the BNO055 driver to look up the bus manager
 *
 * @details
 * The name must match the bus name registered in main.c via rx_bus_manager_register().
 * Passed to rx_bus_manager_get_i2c() to obtain the I2C bus handle.
 *
 * @note Must match the name registered in main.c (currently "i2c1_imu")
 * @warning Do not modify; changing the name at runtime will cause bus lookup failures
 *
 * @since Version 1.0.0
 */
static const char* const s_bus_name = "i2c1_imu";

/* =============================================================================
 * Module-level Offset Constants
 * =============================================================================
 */

/**
 * @enum euler_offsets_t
 * @brief Byte offsets in the 6-byte Euler angle burst-read buffer
 *
 * @details
 * The BNO055 Euler output registers (0x1A-0x1F) are read as a 6-byte burst.
 * Each 16-bit field occupies two consecutive bytes in little-endian order.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_eul_h_lsb_off = 0, /**< Byte offset of heading LSB in euler_buf */
  k_eul_h_msb_off = 1, /**< Byte offset of heading MSB in euler_buf */
  k_eul_r_lsb_off = 2, /**< Byte offset of roll LSB in euler_buf */
  k_eul_r_msb_off = 3, /**< Byte offset of roll MSB in euler_buf */
  k_eul_p_lsb_off = 4, /**< Byte offset of pitch LSB in euler_buf */
  k_eul_p_msb_off = 5, /**< Byte offset of pitch MSB in euler_buf */
} euler_offsets_t;

/**
 * @enum quat_offsets_t
 * @brief Byte offsets in the 8-byte quaternion burst-read buffer
 *
 * @details
 * The BNO055 quaternion output registers (0x20-0x27) are read as an 8-byte burst.
 * Each 16-bit field occupies two consecutive bytes in little-endian order.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_qua_w_lsb_off = 0, /**< Byte offset of quaternion W LSB in quat_buf */
  k_qua_w_msb_off = 1, /**< Byte offset of quaternion W MSB in quat_buf */
  k_qua_x_lsb_off = 2, /**< Byte offset of quaternion X LSB in quat_buf */
  k_qua_x_msb_off = 3, /**< Byte offset of quaternion X MSB in quat_buf */
  k_qua_y_lsb_off = 4, /**< Byte offset of quaternion Y LSB in quat_buf */
  k_qua_y_msb_off = 5, /**< Byte offset of quaternion Y MSB in quat_buf */
  k_qua_z_lsb_off = 6, /**< Byte offset of quaternion Z LSB in quat_buf */
  k_qua_z_msb_off = 7, /**< Byte offset of quaternion Z MSB in quat_buf */
} quat_offsets_t;

/**
 * @enum lia_offsets_t
 * @brief Byte offsets in the 6-byte linear acceleration burst-read buffer
 *
 * @details
 * The BNO055 linear acceleration registers (0x28-0x2D) are read as a 6-byte burst.
 * Each 16-bit field occupies two consecutive bytes in little-endian order.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_lia_x_lsb_off = 0, /**< Byte offset of linear accel X LSB in lia_buf */
  k_lia_x_msb_off = 1, /**< Byte offset of linear accel X MSB in lia_buf */
  k_lia_y_lsb_off = 2, /**< Byte offset of linear accel Y LSB in lia_buf */
  k_lia_y_msb_off = 3, /**< Byte offset of linear accel Y MSB in lia_buf */
  k_lia_z_lsb_off = 4, /**< Byte offset of linear accel Z LSB in lia_buf */
  k_lia_z_msb_off = 5, /**< Byte offset of linear accel Z MSB in lia_buf */
} lia_offsets_t;

/**
 * @enum gyro_offsets_t
 * @brief Byte offsets within a gyroscope burst read buffer (6 bytes)
 *
 * @details
 * Maps byte positions to X/Y/Z LSB and MSB within the 6-byte buffer
 * read from BNO055 registers GYR_DATA_X_LSB (0x14) through GYR_DATA_Z_MSB (0x19).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_gyr_x_lsb_off = 0, /**< Byte offset of gyro X LSB in gyro_buf */
  k_gyr_x_msb_off = 1, /**< Byte offset of gyro X MSB in gyro_buf */
  k_gyr_y_lsb_off = 2, /**< Byte offset of gyro Y LSB in gyro_buf */
  k_gyr_y_msb_off = 3, /**< Byte offset of gyro Y MSB in gyro_buf */
  k_gyr_z_lsb_off = 4, /**< Byte offset of gyro Z LSB in gyro_buf */
  k_gyr_z_msb_off = 5, /**< Byte offset of gyro Z MSB in gyro_buf */
} gyro_offsets_t;

/**
 * @enum bno055_gyro_size_t
 * @brief Expected byte count for a gyroscope burst read
 *
 * @details
 * A BNO055 gyroscope reading consists of X, Y, Z each as a 16-bit
 * little-endian value: 3 components x 2 bytes = 6 bytes total.
 * Used in static_assert to verify the buffer constant is large enough.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_gyro_expected_bytes = 6U, /**< Minimum bytes needed for gyroscope X/Y/Z (3 x 2 bytes) */
} bno055_gyro_size_t;

/**
 * @enum bno055_quat_size_t
 * @brief Expected byte count for a quaternion burst read
 *
 * @details
 * A BNO055 quaternion reading consists of W, X, Y, Z each as a 16-bit
 * little-endian value: 4 components x 2 bytes = 8 bytes total.
 * Used in static_assert to verify the buffer constant is large enough.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_quat_expected_bytes =
    8U, /**< Minimum bytes needed for quaternion W/X/Y/Z (4 x 2 bytes) */
} bno055_quat_size_t;

/**
 * @enum bno055_euler_size_t
 * @brief Expected byte count for an Euler angle burst read
 *
 * @details
 * A BNO055 Euler reading consists of heading, roll, pitch each as a 16-bit
 * little-endian value: 3 components x 2 bytes = 6 bytes total.
 * Used in static_assert to verify the buffer constant is large enough.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_euler_expected_bytes =
    6U, /**< Minimum bytes needed for Euler heading/roll/pitch (3 x 2 bytes) */
} bno055_euler_size_t;

/**
 * @enum bno055_lia_size_t
 * @brief Expected byte count for a linear acceleration burst read
 *
 * @details
 * A BNO055 linear acceleration reading consists of X, Y, Z each as a 16-bit
 * little-endian value: 3 components x 2 bytes = 6 bytes total.
 * Used in static_assert to verify the buffer constant is large enough.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_lia_expected_bytes =
    6U, /**< Minimum bytes needed for linear accel X/Y/Z (3 x 2 bytes) */
} bno055_lia_size_t;

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

RX_STATIC_TESTABLE rx_err_t internal_write_reg(uint8_t reg, uint8_t val);
RX_STATIC_TESTABLE rx_err_t internal_read_regs(uint8_t reg, uint8_t* buf, uint8_t len);
static inline int16_t       internal_assemble_int16_le(uint8_t low, uint8_t high);
RX_STATIC_TESTABLE rx_err_t internal_read_euler(bno055_data_t* out);
RX_STATIC_TESTABLE rx_err_t internal_read_quat(bno055_data_t* out);
RX_STATIC_TESTABLE rx_err_t internal_read_lia(bno055_data_t* out);
RX_STATIC_TESTABLE rx_err_t internal_read_gyro(bno055_data_t* out);
RX_STATIC_TESTABLE rx_err_t internal_init_reset_and_wait(void);
RX_STATIC_TESTABLE rx_err_t internal_init_configure(void);
RX_STATIC_TESTABLE rx_err_t internal_init_enter_ndof(void);
RX_STATIC_TESTABLE rx_err_t internal_verify_chip_id(void);
RX_STATIC_TESTABLE rx_err_t internal_init_enable_interrupt(void);
static rx_err_t             internal_init_run_sequence(void);

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Write a single byte to a BNO055 register via I2C
 *
 * @details
 * Sends a 2-byte I2C write transaction [reg, val] to the BNO055 at
 * device address 0x28 (embedded in the "i2c1_imu" bus configuration).
 *
 *
 *
 * @pre s_manager must be non-NULL (set by rx_bno055_init)
 * @pre "i2c1_imu" bus must be initialized
 * @post Register at address reg contains value val (if k_rx_ok)
 * @post Bus transaction completes before function returns
 *
 * @note Not thread-safe; single-task access assumed
 * @see rx_bus_i2c_write() Underlying I2C write function
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_write_reg(uint8_t reg, uint8_t val)
{
  /* Pre-conditions: s_manager and s_bus_name are non-NULL by construction;
   * called only after rx_bno055_init stores a valid manager and s_bus_name
   * is a compile-time string constant. */
  RX_ASSERT_PRE(s_manager != NULL, "s_manager must be non-NULL before write");
  /* GCOVR_EXCL_BR_START LCOV_EXCL_BR_START(s_bus_name is a compile-time constant) */
  RX_ASSERT_PRE(s_bus_name != NULL, "s_bus_name must be non-NULL before write");
  /* GCOVR_EXCL_BR_STOP LCOV_EXCL_BR_STOP */
  uint8_t buf[k_bno055_write_buf_size];
  buf[k_bno055_write_idx_reg] = reg;
  buf[k_bno055_write_idx_val] = val;
  return rx_bus_i2c_write(s_manager, s_bus_name, buf, k_bno055_write_buf_size);
}

/**
 * @brief Burst-read consecutive registers from BNO055 via I2C write-read
 *
 * @details
 * Issues an I2C combined write-read (repeated start) transaction:
 * 1. Write the starting register address (1 byte)
 * 2. Read len bytes of register data
 *
 * The BNO055 auto-increments the register address for consecutive reads.
 *
 *
 *
 * @pre s_manager must be non-NULL (set by rx_bno055_init)
 * @pre "i2c1_imu" bus must be initialized
 * @pre buf must have capacity for len bytes
 * @post buf[0..len-1] contain register data starting at reg (if k_rx_ok)
 * @post Bus transaction completes before function returns
 *
 * @note Not thread-safe; single-task access assumed
 * @see rx_bus_i2c_write_read() Underlying I2C combined transaction
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_read_regs(uint8_t reg, uint8_t* buf, uint8_t len)
{
  /* Pre-conditions: s_manager is non-NULL (only called after successful init);
   * buf is a local stack buffer (never NULL); len is an enum constant (always > 0). */
  RX_ASSERT_PRE(s_manager != NULL, "s_manager must be non-NULL before read");
  RX_ASSERT_PRE(buf != NULL, "buf must be non-NULL for read operation");
  RX_ASSERT_PRE(len > 0U, "len must be positive for read operation");
  return rx_bus_i2c_write_read(s_manager, s_bus_name, &reg, k_bno055_read_cmd_size, buf, len);
}

/**
 * @brief Assemble a little-endian 16-bit signed integer from two bytes
 *
 * @details
 * The BNO055 outputs all multi-byte values in little-endian format (LSB first).
 * This helper combines a low byte and a high byte into a signed int16_t value
 * using unsigned arithmetic to avoid implementation-defined behaviour.
 *
 *
 *
 * @pre low and high are valid bytes read from BNO055 registers
 * @pre uint16_t is exactly 16 bits (verified by static_assert below)
 * @post Return value has correct sign via two's complement reinterpretation
 * @post Two static_asserts below enforce compile-time invariants on shift and width
 *
 * @note Inline function; zero overhead after compiler optimization
 * @see bno055_byte_idx_t LSB/MSB index constants
 *
 * @par NASA Power of 10 Rule 5 Compliance:
 * This single-expression inline helper has no runtime state or I/O side effects.
 * Both preconditions and postconditions are enforced by the two static_asserts
 * inside the function body. Runtime assertion overhead on uint8_t parameters
 * is not warranted; callers are responsible for providing valid register bytes.
 *
 * @since Version 1.0.0
 */
static inline int16_t internal_assemble_int16_le(uint8_t low, uint8_t high)
{
  static_assert(
    (bool)(((unsigned)k_bno055_shift_msb == (unsigned)k_bno055_expected_msb_shift) != 0),
    "MSB shift must be 8 for little-endian assembly");
  static_assert(
    (bool)((sizeof(uint16_t) * CHAR_BIT == (unsigned)k_bno055_expected_uint16_bits) != 0),
    "uint16_t must be 16 bits for assembly to be well-defined");
  return (int16_t)((uint16_t)low | ((uint16_t)high << k_bno055_shift_msb));
}

/* =============================================================================
 * Init Helpers
 * =============================================================================
 */

/**
 * @brief BNO055 init steps 1-2: software reset, POR delay, CONFIG mode, config delay
 *
 * @details
 * Writes SYS_TRIGGER reset command and waits 650 ms for POR sequence to
 * complete. Then enters CONFIG mode and waits 20 ms for the transition.
 * Must be called first in the init sequence before any register writes.
 *
 *
 * @pre s_manager non-NULL
 * @pre "i2c1_imu" bus initialized
 * @post Sensor in CONFIG mode, ready for configuration register writes
 * @post ~670 ms elapsed (650 ms POR + 20 ms CONFIG transition)
 *
 * @note Not thread-safe; called only from rx_bno055_init()
 * @note Blocking: 65 ticks @ 10 ms/tick = 650 ms POR, then 2 ticks = 20 ms CONFIG
 *
 * @see bno055_delay_ms_t Timing constants
 * @see bno055_sys_trigger_t Reset command values
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_init_reset_and_wait(void)
{
  /* Pre-conditions: called only from rx_bno055_init after s_manager is set
   * and s_bus_name is a compile-time string constant (never NULL). */
  RX_ASSERT_PRE(s_manager != NULL, "s_manager must be non-NULL for reset");
  /* GCOVR_EXCL_BR_START LCOV_EXCL_BR_START(s_bus_name is a compile-time constant) */
  RX_ASSERT_PRE(s_bus_name != NULL, "s_bus_name must be non-NULL for reset sequence");
  /* GCOVR_EXCL_BR_STOP LCOV_EXCL_BR_STOP */

  /* Step 1: Software reset, then wait for POR sequence */
  rx_err_t err = internal_write_reg(k_bno055_reg_sys_trigger, k_bno055_sys_trigger_rst);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Software reset failed");
    return err;
  }
  (void)tx_thread_sleep(
    (ULONG)(k_bno055_delay_por_ms / k_bno055_ms_per_tick)); /* 65 ticks @ 10 ms/tick = 650 ms */

  /* Step 2: Enter CONFIG mode (required for configuration register writes) */
  err = internal_write_reg(k_bno055_reg_opr_mode, k_bno055_opr_config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set CONFIG mode failed");
    return err;
  }
  (void)tx_thread_sleep(
    (ULONG)k_bno055_delay_config_ms / (ULONG)k_bno055_ms_per_tick +
    (ULONG)k_bno055_tick_round_up); /* 2 ticks @ 10 ms/tick = 20 ms (round up for 19 ms) */

  return k_rx_ok;
}

/**
 * @brief BNO055 init steps 3-6: configure power mode, units, axis map, clear trigger
 *
 * @details
 * Writes power mode (Normal), unit selection (degrees/m/s^2/dps/Celsius),
 * axis remapping (standard orientation), and clears the system trigger register.
 * All writes are performed in CONFIG mode (entered by internal_init_reset_and_wait).
 *
 *
 * @pre s_manager non-NULL
 * @pre BNO055 in CONFIG mode (internal_init_reset_and_wait succeeded)
 * @post Power mode set to Normal
 * @post Units set to degrees, m/s^2, dps, Celsius
 * @post Axis map set to standard orientation
 * @post System trigger cleared
 *
 * @note Not thread-safe; called only from rx_bno055_init()
 *
 * @see bno055_pwr_mode_t Power mode constants
 * @see bno055_axis_map_t Axis map constants
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_init_configure(void)
{
  /* Pre-conditions: called only after rx_bno055_init sets a valid manager;
   * s_bus_name is a compile-time string constant (never NULL). */
  RX_ASSERT_PRE(s_manager != NULL, "s_manager must be non-NULL for configure");
  /* GCOVR_EXCL_BR_START LCOV_EXCL_BR_START(s_bus_name is a compile-time constant) */
  RX_ASSERT_PRE(s_bus_name != NULL, "s_bus_name must be non-NULL for configure");
  /* GCOVR_EXCL_BR_STOP LCOV_EXCL_BR_STOP */

  /* Step 3: Set normal power mode */
  rx_err_t err = internal_write_reg(k_bno055_reg_pwr_mode, k_bno055_pwr_normal);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set power mode failed");
    return err;
  }

  /* Step 4: Set default measurement units (degrees, m/s^2, dps, Celsius) */
  err = internal_write_reg(k_bno055_reg_unit_sel, k_bno055_unit_sel_default);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set unit sel failed");
    return err;
  }

  /* Step 5: Set default axis remapping */
  err = internal_write_reg(k_bno055_reg_axis_map_cfg, k_bno055_axis_map_default);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set axis map cfg failed");
    return err;
  }

  err = internal_write_reg(k_bno055_reg_axis_map_sgn, k_bno055_axis_map_sign_pos);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set axis map sign failed");
    return err;
  }

  /* Step 6: Clear system trigger register */
  err = internal_write_reg(k_bno055_reg_sys_trigger, k_bno055_sys_trigger_clear);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Clear sys trigger failed");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief BNO055 init step 7: enter NDOF fusion mode and wait for transition
 *
 * @details
 * Writes OPR_MODE = NDOF (0x0C) to activate full 9-DOF sensor fusion
 * (accelerometer + gyroscope + magnetometer). Waits 10 ms (1 tick) for
 * the CONFIG -> NDOF transition to complete per BNO055 datasheet.
 *
 *
 * @pre s_manager non-NULL
 * @pre BNO055 configured (internal_init_configure succeeded)
 * @post OPR_MODE register set to NDOF (0x0C)
 * @post ~10 ms elapsed (1 tick) for mode transition to settle
 *
 * @note Not thread-safe; called only from rx_bno055_init()
 * @note Blocking: 1 tick @ 10 ms/tick = 10 ms (rounds up from 7 ms minimum)
 *
 * @see bno055_opr_mode_t Operating mode constants
 * @see bno055_delay_ticks_t Tick delay constants
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_init_enter_ndof(void)
{
  /* Pre-conditions: called only after rx_bno055_init sets a valid manager;
   * s_bus_name is a compile-time string constant (never NULL). */
  RX_ASSERT_PRE(s_manager != NULL, "s_manager must be non-NULL for NDOF entry");
  /* GCOVR_EXCL_BR_START LCOV_EXCL_BR_START(s_bus_name is a compile-time constant) */
  RX_ASSERT_PRE(s_bus_name != NULL, "s_bus_name must be non-NULL");
  /* GCOVR_EXCL_BR_STOP LCOV_EXCL_BR_STOP */

  /* Step 7: Enter NDOF fusion mode (full 9-DOF sensor fusion) */
  const rx_err_t err = internal_write_reg(k_bno055_reg_opr_mode, k_bno055_opr_ndof);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set NDOF mode failed");
    return err;
  }
  (void)tx_thread_sleep(
    (ULONG)k_bno055_delay_ndof_ticks); /* 1 tick @ 10 ms/tick = 10 ms (rounds up from 7 ms) */

  return k_rx_ok;
}

/**
 * @brief BNO055 init step 8: read CHIP_ID register and verify it is 0xA0
 *
 * @details
 * Reads the CHIP_ID register (0x00) and compares against k_bno055_chip_id_expected (0xA0).
 * A mismatch indicates a wrong device, wiring fault, or I2C address collision.
 * Returns k_rx_err_invalid_state if the chip ID does not match.
 *
 *
 * @pre s_manager non-NULL
 * @pre BNO055 in NDOF mode (internal_init_enter_ndof succeeded)
 * @post CHIP_ID register confirmed == 0xA0 on k_rx_ok
 *
 * @note Not thread-safe; called only from rx_bno055_init()
 *
 * @see bno055_chip_id_t Expected chip ID constant
 * @see rx_bno055_init() Calls this as the final verification step
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_verify_chip_id(void)
{
  /* Pre-conditions: called only after rx_bno055_init sets s_manager;
   * s_bus_name is a compile-time string constant (never NULL). */
  RX_ASSERT_PRE(s_manager != NULL, "s_manager must be non-NULL for chip ID verify");
  /* GCOVR_EXCL_BR_START LCOV_EXCL_BR_START(s_bus_name is a compile-time constant) */
  RX_ASSERT_PRE(s_bus_name != NULL, "s_bus_name must be non-NULL for chip ID verification");
  /* GCOVR_EXCL_BR_STOP LCOV_EXCL_BR_STOP */

  /* Step 8: Verify chip ID */
  uint8_t  chip_id = 0;
  rx_err_t err     = internal_read_regs(k_bno055_reg_chip_id, &chip_id, k_bno055_single_byte);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Chip ID read failed");
    return err;
  }

  if (chip_id != k_bno055_chip_id_expected) {
    rx_log_error_val(s_tag, "Unexpected chip ID", (uint32_t)chip_id);
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Configure BNO055 Page 1 interrupt engine to assert INT on each data sample
 *
 * @details
 * Writes to the BNO055 Page 1 interrupt registers to enable the accelerometer
 * BSX data-ready interrupt (ACC_BSX_DRDY, INT_EN/INT_MSK bit 0). This causes
 * the INT pin to assert on every new accelerometer sample (~100 Hz in NDOF mode),
 * providing a hardware data-ready signal without any threshold configuration.
 *
 * Register write sequence (BNO055 must be in CONFIG mode):
 * 1. PAGE_ID = 1 (0x07) -- switch to Page 1
 * 2. INT_MSK = 0x01 (Page 1, 0x0F) -- route ACC_BSX_DRDY to INT pin (bit 0)
 * 3. INT_EN  = 0x01 (Page 1, 0x10) -- enable ACC_BSX_DRDY interrupt source (bit 0)
 * 4. PAGE_ID = 0 (0x07) -- restore Page 0 for normal sensor reads
 *
 * Must be called while the sensor is in CONFIG mode (before internal_init_enter_ndof)
 * so the interrupt registers accept writes per BNO055 datasheet section 4.3.
 *
 * If any write fails, a best-effort restore of PAGE_ID=0 is attempted so the
 * sensor is left in a known page before returning the error. On error, the caller
 * (rx_bno055_init) will clear s_manager and return the error.
 *
 *
 * @pre s_manager non-NULL (set by rx_bno055_init before calling this helper)
 * @pre BNO055 in CONFIG mode -- call before internal_init_enter_ndof() per BNO055 datasheet
 * @post INT pin will assert on each BNO055 accelerometer data-ready event (on k_rx_ok)
 * @post PAGE_ID restored to 0 (attempted even on error)
 *
 * @note Not thread-safe; called only from rx_bno055_init()
 *
 * @see bno055_int_reg_t Page 1 register address constants
 * @see bno055_int_en_t INT_EN enable bit value (k_bno055_int_en_acc_bsx_drdy)
 * @see bno055_int_msk_t INT_MSK route bit value (k_bno055_int_msk_acc_bsx_drdy)
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_init_enable_interrupt(void)
{
  /* Pre-conditions: called only after rx_bno055_init sets a valid manager;
   * s_bus_name is a compile-time string constant (never NULL). */
  RX_ASSERT_PRE(s_manager != NULL, "s_manager must be non-NULL for interrupt enable");
  /* GCOVR_EXCL_BR_START LCOV_EXCL_BR_START(s_bus_name is a compile-time constant) */
  RX_ASSERT_PRE(s_bus_name != NULL, "s_bus_name must be non-NULL for interrupt enable");
  /* GCOVR_EXCL_BR_STOP LCOV_EXCL_BR_STOP */

  /* Switch to Page 1 to access interrupt engine registers */
  rx_err_t err = internal_write_reg(k_bno055_reg_page_id, k_bno055_page1);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "INT init: page 1 switch failed");
    return err;
  }

  /* Route ACC_BSX_DRDY interrupt source to INT pin (INT_MSK bit 0) */
  err = internal_write_reg(k_bno055_reg_int_msk, k_bno055_int_msk_acc_bsx_drdy);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "INT init: INT_MSK write failed");
    (void)internal_write_reg(k_bno055_reg_page_id, k_bno055_page0);
    return err;
  }

  /* Enable accelerometer BSX data-ready as an interrupt source (INT_EN bit 0) */
  err = internal_write_reg(k_bno055_reg_int_en, k_bno055_int_en_acc_bsx_drdy);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "INT init: INT_EN write failed");
    (void)internal_write_reg(k_bno055_reg_page_id, k_bno055_page0);
    return err;
  }

  /* Restore Page 0 for normal sensor output reads */
  err = internal_write_reg(k_bno055_reg_page_id, k_bno055_page0);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "INT init: page 0 restore failed");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Execute the multi-step BNO055 initialization sequence
 *
 * @details
 * Runs reset, configure, optional interrupt setup, NDOF mode entry, and chip
 * ID verification. Extracted from rx_bno055_init() to keep that function below
 * the readability-function-size statement threshold.
 *
 *
 * @pre s_manager non-NULL (set by caller before invoking)
 * @pre s_mode set to desired operating mode by caller
 * @post All BNO055 init registers written; NDOF mode active on k_rx_ok
 * @post Chip ID verified as 0xA0 on k_rx_ok
 *
 * @note Not thread-safe; called only from rx_bno055_init()
 *
 * @see internal_init_reset_and_wait() Steps 1-2
 * @see internal_init_configure() Steps 3-6
 * @see internal_init_enter_ndof() Step 7
 * @see internal_verify_chip_id() Step 8
 * @see internal_init_enable_interrupt() Interrupt engine (interrupt mode only)
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_init_run_sequence(void)
{
  rx_err_t err = internal_init_reset_and_wait();
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_init_configure();
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure interrupt engine while still in CONFIG mode (BNO055 requires this) */
  if (s_mode == k_bno055_mode_interrupt) {
    err = internal_init_enable_interrupt();
    if (err != k_rx_ok) {
      return err;
    }
  }

  err = internal_init_enter_ndof();
  if (err != k_rx_ok) {
    return err;
  }

  return internal_verify_chip_id();
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize BNO055 sensor and configure for NDOF fusion mode
 *
 * @details
 * Validates parameters, stores module state, then delegates the complete
 * 8-step initialization sequence (BNO055 datasheet section 3.3.1) to
 * internal_init_run_sequence(). Any failure sets s_manager = NULL before
 * returning so the module is re-initializable.
 *
 * Delay rationale:
 * - 650 ms after reset: BNO055 internal boot sequence (ARM Cortex-M0 startup)
 * - 19 ms after config mode: Fusion -> CONFIG mode transition
 * - 7 ms after NDOF mode: CONFIG -> NDOF mode transition
 *
 * ThreadX tick conversion: 1 tick = 10 ms at 100 Hz tick rate (k_bno055_ms_per_tick = 10).
 * - 650 ms POR: 650/10 = 65 ticks
 * - 19 ms config: 19/10 + 1 = 2 ticks (20 ms, rounded up for margin)
 * - 7 ms NDOF: 1 tick (10 ms, rounded up for margin)
 *
 *
 *
 * @pre manager non-NULL with "i2c1_imu" bus registered and initialized
 * @pre config non-NULL with a valid bno055_mode_t value in config->mode
 * @post s_initialized == true on success
 * @post Sensor running NDOF fusion
 * @post s_manager == NULL on any failure path
 * @post INT pin asserts at accelerometer data rate when config->mode == k_bno055_mode_interrupt
 *
 * @note Not thread-safe
 * @note Blocks ~700 ms total
 *
 * @see internal_init_run_sequence() Delegated initialization sequence
 * @see bno055_delay_ms_t Timing constants
 * @see bno055_config_t Configuration struct
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bno055_init(rx_bus_manager_t* manager, const bno055_config_t* config)
{
  static_assert((bool)((k_bno055_ms_per_tick != 0) != 0),
                "k_bno055_ms_per_tick must be non-zero to avoid division by zero in tick delays");
  RX_CHECK_NULL_PTR(manager, s_tag, "Bus manager is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "Config is NULL");

  if (s_initialized) {
    rx_log_error(s_tag, "BNO055 already initialized");
    return k_rx_err_invalid_state;
  }

  s_manager     = manager;
  s_mode        = config->mode;
  s_initialized = false;

  const rx_err_t err = internal_init_run_sequence();
  if (err != k_rx_ok) {
    s_manager = NULL;
    return err;
  }

  s_initialized = true;
  rx_log_info(s_tag, "BNO055 initialized in NDOF mode");

  return k_rx_ok;
}

/**
 * @brief Read Euler angle registers from BNO055 and populate heading/roll/pitch
 *
 * @details
 * Burst-reads 6 bytes from register 0x1A (EUL_H_LSB) and assembles
 * three 16-bit little-endian values into out->heading_deg16, roll_deg16, pitch_deg16.
 *
 *
 *
 * @pre s_initialized == true
 * @pre out non-NULL
 * @post out->heading_deg16, roll_deg16, pitch_deg16 updated
 *
 * @note Called only from rx_bno055_read()
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_read_euler(bno055_data_t* out)
{
  /* Pre-conditions: s_initialized is true (rx_bno055_read checks via
   * RX_CHECK_INITIALIZED); out is a local stack variable (never NULL). */
  RX_ASSERT_PRE(s_initialized, "driver must be initialized before reading Euler angles");
  RX_ASSERT_PRE(out != NULL, "out must not be NULL");
  static_assert(
    (bool)(((unsigned)k_bno055_euler_bytes >= (unsigned)k_bno055_euler_expected_bytes) != 0),
    "euler buffer must hold 6 bytes");

  uint8_t  euler_buf[k_bno055_euler_bytes];
  rx_err_t err = internal_read_regs(k_bno055_reg_eul_h_lsb, euler_buf, k_bno055_euler_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Euler read failed");
    return err;
  }

  out->heading_deg16 =
    internal_assemble_int16_le(euler_buf[k_eul_h_lsb_off], euler_buf[k_eul_h_msb_off]);
  out->roll_deg16 =
    internal_assemble_int16_le(euler_buf[k_eul_r_lsb_off], euler_buf[k_eul_r_msb_off]);
  out->pitch_deg16 =
    internal_assemble_int16_le(euler_buf[k_eul_p_lsb_off], euler_buf[k_eul_p_msb_off]);
  return k_rx_ok;
}

/**
 * @brief Read quaternion registers from BNO055 and populate quat_w/x/y/z
 *
 * @details
 * Burst-reads 8 bytes from register 0x20 (QUA_W_LSB) and assembles
 * four 16-bit little-endian values into the quat_w/x/y/z fields.
 *
 *
 *
 * @pre s_initialized == true
 * @pre out non-NULL
 * @post out->quat_w/x/y/z updated
 *
 * @note Called only from rx_bno055_read()
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_read_quat(bno055_data_t* out)
{
  /* Pre-conditions: s_initialized is true (rx_bno055_read checks via
   * RX_CHECK_INITIALIZED); out is a local stack variable (never NULL). */
  RX_ASSERT_PRE(s_initialized, "driver must be initialized before reading quaternion");
  RX_ASSERT_PRE(out != NULL, "out must not be NULL");
  static_assert(
    (bool)(((unsigned)k_bno055_quat_bytes >= (unsigned)k_bno055_quat_expected_bytes) != 0),
    "quat buffer must hold 8 bytes");

  uint8_t  quat_buf[k_bno055_quat_bytes];
  rx_err_t err = internal_read_regs(k_bno055_reg_qua_w_lsb, quat_buf, k_bno055_quat_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Quaternion read failed");
    return err;
  }

  out->quat_w = internal_assemble_int16_le(quat_buf[k_qua_w_lsb_off], quat_buf[k_qua_w_msb_off]);
  out->quat_x = internal_assemble_int16_le(quat_buf[k_qua_x_lsb_off], quat_buf[k_qua_x_msb_off]);
  out->quat_y = internal_assemble_int16_le(quat_buf[k_qua_y_lsb_off], quat_buf[k_qua_y_msb_off]);
  out->quat_z = internal_assemble_int16_le(quat_buf[k_qua_z_lsb_off], quat_buf[k_qua_z_msb_off]);
  return k_rx_ok;
}

/**
 * @brief Read linear acceleration registers from BNO055 and populate lin_acc_x/y/z
 *
 * @details
 * Burst-reads 6 bytes from register 0x28 (LIA_X_LSB) and assembles
 * three 16-bit little-endian values into the lin_acc_x/y/z fields.
 *
 *
 *
 * @pre s_initialized == true
 * @pre out non-NULL
 * @post out->lin_acc_x/y/z updated
 *
 * @note Called only from rx_bno055_read()
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_read_lia(bno055_data_t* out)
{
  /* Pre-conditions: s_initialized is true (rx_bno055_read checks via
   * RX_CHECK_INITIALIZED); out is a local stack variable (never NULL). */
  RX_ASSERT_PRE(s_initialized, "driver must be initialized before reading linear acceleration");
  RX_ASSERT_PRE(out != NULL, "out must not be NULL");
  static_assert(
    (bool)(((unsigned)k_bno055_lia_bytes >= (unsigned)k_bno055_lia_expected_bytes) != 0),
    "lia buffer must hold 6 bytes");

  uint8_t  lia_buf[k_bno055_lia_bytes];
  rx_err_t err = internal_read_regs(k_bno055_reg_lia_x_lsb, lia_buf, k_bno055_lia_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Linear accel read failed");
    return err;
  }

  out->lin_acc_x = internal_assemble_int16_le(lia_buf[k_lia_x_lsb_off], lia_buf[k_lia_x_msb_off]);
  out->lin_acc_y = internal_assemble_int16_le(lia_buf[k_lia_y_lsb_off], lia_buf[k_lia_y_msb_off]);
  out->lin_acc_z = internal_assemble_int16_le(lia_buf[k_lia_z_lsb_off], lia_buf[k_lia_z_msb_off]);
  return k_rx_ok;
}

/**
 * @brief Read BNO055 gyroscope data (raw, 16-bit little-endian)
 *
 * @details
 * Performs a 6-byte I2C burst read from GYR_DATA_X_LSB (0x14) through
 * GYR_DATA_Z_MSB (0x19) and assembles three signed 16-bit values.
 * Scale: 1 LSB = 1/16 deg/s (k_bno055_scale_gyro_lsb_per_dps = 16)
 * when k_bno055_unit_sel_default is configured (dps mode, default).
 *
 *
 *
 * @pre s_initialized must be true (call rx_bno055_init() first)
 * @pre out must not be NULL
 * @post out->gyro_x_dps16, gyro_y_dps16, gyro_z_dps16 populated on k_rx_ok
 *
 * @note Not thread-safe; called only from rx_bno055_read()
 *
 * @see rx_bno055_read() Caller
 * @see k_bno055_scale_gyro_lsb_per_dps Scale factor (16 LSB per deg/s)
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_read_gyro(bno055_data_t* out)
{
  /* Pre-conditions: s_initialized is true (rx_bno055_read checks via
   * RX_CHECK_INITIALIZED); out is a local stack variable (never NULL). */
  RX_ASSERT_PRE(s_initialized, "driver must be initialized before reading gyro");
  RX_ASSERT_PRE(out != NULL, "out must not be NULL");
  static_assert(
    (bool)(((unsigned)k_bno055_gyro_bytes >= (unsigned)k_bno055_gyro_expected_bytes) != 0),
    "gyro buffer must hold 6 bytes");

  uint8_t  gyro_buf[k_bno055_gyro_bytes];
  rx_err_t err = internal_read_regs(k_bno055_reg_gyr_x_lsb, gyro_buf, k_bno055_gyro_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Gyro read failed");
    return err;
  }

  out->gyro_x_dps16 =
    internal_assemble_int16_le(gyro_buf[k_gyr_x_lsb_off], gyro_buf[k_gyr_x_msb_off]);
  out->gyro_y_dps16 =
    internal_assemble_int16_le(gyro_buf[k_gyr_y_lsb_off], gyro_buf[k_gyr_y_msb_off]);
  out->gyro_z_dps16 =
    internal_assemble_int16_le(gyro_buf[k_gyr_z_lsb_off], gyro_buf[k_gyr_z_msb_off]);
  return k_rx_ok;
}

/**
 * @brief Read current BNO055 fusion output data
 *
 * @details
 * Performs six sequential I2C burst reads to collect all output data.
 * All 16-bit values are assembled in little-endian format as specified
 * in the BNO055 datasheet (LSB register first, MSB register second).
 *
 * Register read sequence:
 * 1. 0x14: 6 bytes -> Gyroscope X[2], Y[2], Z[2]
 * 2. 0x1A: 6 bytes -> Euler heading[2], roll[2], pitch[2]
 * 3. 0x20: 8 bytes -> Quaternion W[2], X[2], Y[2], Z[2]
 * 4. 0x28: 6 bytes -> Linear accel X[2], Y[2], Z[2]
 * 5. 0x34: 1 byte  -> Temperature
 * 6. 0x35: 1 byte  -> Calibration status
 *
 *
 *
 * @pre rx_bno055_init() succeeded
 * @pre out non-NULL
 * @post All fields in *out populated on k_rx_ok
 * @post out->calib_stat reflects current calibration quality
 *
 * @note Not thread-safe
 *
 * @see bno055_scale_t Conversion factors for physical units
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bno055_read(bno055_data_t* out)
{
  RX_CHECK_NULL_PTR(out, s_tag, "Output data pointer is NULL");

  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  rx_err_t err = internal_read_gyro(out);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_read_euler(out);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_read_quat(out);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_read_lia(out);
  if (err != k_rx_ok) {
    return err;
  }

  /* Read Temperature: 1 byte from 0x34 */
  uint8_t temp_raw = 0;
  err              = internal_read_regs(k_bno055_reg_temp, &temp_raw, k_bno055_single_byte);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Temperature read failed");
    return err;
  }

  /* Read Calibration status: 1 byte from 0x35 */
  uint8_t calib_raw = 0;
  err               = internal_read_regs(k_bno055_reg_calib_stat, &calib_raw, k_bno055_single_byte);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Calib status read failed");
    return err;
  }

  out->temp_degc  = (int8_t)temp_raw;
  out->calib_stat = calib_raw;

  return k_rx_ok;
}

/**
 * @brief Check whether all BNO055 subsystems are fully calibrated
 *
 * @details
 * Reads CALIB_STAT register and checks all four 2-bit fields are at
 * level 3 (fully calibrated).
 *
 *
 *
 * @pre rx_bno055_init() succeeded
 * @pre out_calibrated non-NULL
 * @post *out_calibrated valid only on k_rx_ok
 * @post CALIB_STAT register read (no state modified)
 *
 * @note Not thread-safe
 * @see bno055_calib_shift_t Calibration shift constants
 * @see bno055_calib_mask_t Calibration mask and full-calibration sentinel
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bno055_is_calibrated(bool* out_calibrated)
{
  RX_CHECK_NULL_PTR(out_calibrated, s_tag, "Output calibrated pointer is NULL");

  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }

  uint8_t  calib_raw = 0;
  rx_err_t err = internal_read_regs(k_bno055_reg_calib_stat, &calib_raw, k_bno055_single_byte);
  if (err != k_rx_ok) {
    return err;
  }

  const uint8_t mask = k_bno055_calib_level_mask;
  const uint8_t full = k_bno055_calib_full;

  const uint8_t sys_cal = (calib_raw >> k_bno055_calib_sys_shift) & mask;
  const uint8_t gyr_cal = (calib_raw >> k_bno055_calib_gyr_shift) & mask;
  const uint8_t acc_cal = (calib_raw >> k_bno055_calib_acc_shift) & mask;
  const uint8_t mag_cal = (calib_raw >> k_bno055_calib_mag_shift) & mask;

  /* Use bitwise & to evaluate all sub-expressions without short-circuit,
   * ensuring full branch coverage. */
  *out_calibrated =
    (bool)(((sys_cal == full) & (gyr_cal == full) & (acc_cal == full) & (mag_cal == full)) != 0);

  return k_rx_ok;
}

rx_err_t rx_bno055_clear_int(void)
{
  if (!s_initialized) {
    return k_rx_err_not_initialized;
  }
  uint8_t int_sta = 0U;
  return internal_read_regs(k_bno055_reg_int_sta, &int_sta, k_bno055_single_byte);
}

#ifdef UNIT_TEST
/**
 * @brief Reset all driver static state to uninitialized (unit test only)
 *
 * @details
 * Clears s_initialized and s_manager so that each unit test starts from a
 * clean, uninitialized state. Compiled only when UNIT_TEST is defined.
 * Call from setUp() in test files that exercise the BNO055 driver.
 *
 * @pre None
 * @post s_initialized == false
 * @post s_manager == NULL
 * @post s_mode == k_bno055_mode_poll
 *
 * @note For unit tests only; not compiled in firmware builds
 * @since Version 1.0.0
 */
void rx_bno055_test_reset_state(void)
{
  s_initialized = false;
  s_manager     = NULL;
  s_mode        = k_bno055_mode_poll;
  /* Post-conditions: s_initialized==false, s_manager==NULL, s_mode==poll
   * are guaranteed by the direct assignments above. */
  /* GCOVR_EXCL_BR_START LCOV_EXCL_BR_START(values assigned on lines above) */
  RX_ASSERT_POST(!s_initialized, "s_initialized must be false after reset");
  RX_ASSERT_POST(s_manager == NULL, "s_manager must be NULL after reset");
  /* GCOVR_EXCL_BR_STOP LCOV_EXCL_BR_STOP */
}
#endif /* UNIT_TEST */
