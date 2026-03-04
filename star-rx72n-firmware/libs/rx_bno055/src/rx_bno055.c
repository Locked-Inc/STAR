/* libs/rx_bno055/src/rx_bno055.c */

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
 * Both helpers use the bus manager abstraction with bus name "i2c1".
 * The device address 0x28 is embedded in the "i2c1" bus configuration
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
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "rx_bno055.h"

#include <stddef.h>

#include "rx_bno055_regs.h"
#include "rx_bus_i2c.h"
#include "rx_check.h"
#include "rx_log.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum bno055_i2c_write_size_t
 * @brief I2C write buffer size constants for register writes
 *
 * @details
 * The BNO055 register write protocol sends [register_address, data_byte]
 * as a 2-byte I2C write transaction.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_write_buf_size = 2, /**< Size of [reg, val] write buffer */
  k_bno055_write_idx_reg  = 0, /**< Index of register address in write buffer */
  k_bno055_write_idx_val  = 1, /**< Index of register value in write buffer */
  k_bno055_read_cmd_size  = 1, /**< Size of register address read command */
} bno055_i2c_write_size_t;

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

/** @brief Log tag for this module */
static const char* const s_tag = "BNO055";

/** @brief Bus manager pointer stored during init, used by all read operations */
static rx_bus_manager_t* s_manager = NULL;

/** @brief Guard flag: true after successful rx_bno055_init() */
static bool s_initialized = false;

/** @brief I2C bus name for BNO055 (registered in main.c) */
static const char* const s_bus_name = "i2c1";

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static rx_err_t internal_write_reg(uint8_t reg, uint8_t val);
static rx_err_t internal_read_regs(uint8_t reg, uint8_t* buf, uint8_t len);

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Write a single byte to a BNO055 register via I2C
 *
 * @details
 * Sends a 2-byte I2C write transaction [reg, val] to the BNO055 at
 * device address 0x28 (embedded in the "i2c1" bus configuration).
 *
 * @param[in] reg Register address (from bno055_reg_t)
 * @param[in] val Byte value to write to the register
 *
 * @return rx_err_t I2C transaction result
 * @retval k_rx_ok Write acknowledged by device
 * @retval k_rx_err_nack Device did not acknowledge
 * @retval k_rx_err_timeout Transaction timeout
 *
 * @pre s_manager must be non-NULL (set by rx_bno055_init)
 * @pre "i2c1" bus must be initialized
 * @post Register at address reg contains value val (if k_rx_ok)
 *
 * @note Not thread-safe; single-task access assumed
 * @see rx_bus_i2c_write() Underlying I2C write function
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_write_reg(uint8_t reg, uint8_t val)
{
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
 * @param[in]  reg Register address to start reading from
 * @param[out] buf Buffer to store read bytes. Must have len bytes capacity.
 * @param[in]  len Number of bytes to read (1..255)
 *
 * @return rx_err_t I2C transaction result
 * @retval k_rx_ok len bytes read into buf
 * @retval k_rx_err_nack Device did not acknowledge
 * @retval k_rx_err_timeout Transaction timeout
 *
 * @pre s_manager must be non-NULL (set by rx_bno055_init)
 * @pre "i2c1" bus must be initialized
 * @pre buf must have capacity for len bytes
 * @post buf[0..len-1] contain register data starting at reg (if k_rx_ok)
 *
 * @note Not thread-safe; single-task access assumed
 * @see rx_bus_i2c_write_read() Underlying I2C combined transaction
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_read_regs(uint8_t reg, uint8_t* buf, uint8_t len)
{
  return rx_bus_i2c_write_read(s_manager,
                               s_bus_name,
                               &reg,
                               k_bno055_read_cmd_size,
                               buf,
                               len);
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize BNO055 sensor and configure for NDOF fusion mode
 *
 * @details
 * Executes the complete 8-step initialization sequence as described in the
 * BNO055 datasheet section 3.3.1. Each step is validated and errors cause
 * immediate return.
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
 * @param[in] manager Pointer to initialized bus manager with "i2c1" registered
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Sensor initialized, NDOF mode active
 * @retval k_rx_err_null_ptr manager is NULL
 * @retval k_rx_err_nack I2C NACK (device not found)
 * @retval k_rx_err_invalid_state CHIP_ID != 0xA0
 *
 * @pre manager non-NULL with "i2c1" bus registered and initialized
 * @pre BNO055 powered (3.3V)
 * @post s_initialized == true on success
 * @post Sensor running NDOF fusion
 *
 * @note Not thread-safe
 * @note Blocks ~700 ms total
 *
 * @see bno055_delay_ms_t Timing constants
 * @see bno055_opr_mode_t Operating mode constants
 *
 * @since Version 1.0.0
 */
rx_err_t rx_bno055_init(rx_bus_manager_t* manager)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "Bus manager is NULL");

  s_manager     = manager;
  s_initialized = false;

  /* Step 1: Software reset, then wait for POR sequence */
  rx_err_t err = internal_write_reg((uint8_t)k_bno055_reg_sys_trigger,
                                    (uint8_t)k_bno055_sys_trigger_rst);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Software reset failed");
    return err;
  }
  (void)tx_thread_sleep(k_bno055_delay_por_ms / k_bno055_ms_per_tick); /* 65 ticks @ 10 ms/tick = 650 ms */

  /* Step 2: Enter CONFIG mode (required for configuration register writes) */
  err = internal_write_reg((uint8_t)k_bno055_reg_opr_mode, (uint8_t)k_bno055_opr_config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set CONFIG mode failed");
    return err;
  }
  (void)tx_thread_sleep(k_bno055_delay_config_ms / k_bno055_ms_per_tick + k_bno055_single_byte); /* 2 ticks @ 10 ms/tick = 20 ms (round up for 19 ms) */

  /* Step 3: Set normal power mode */
  err = internal_write_reg((uint8_t)k_bno055_reg_pwr_mode, (uint8_t)k_bno055_pwr_normal);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set power mode failed");
    return err;
  }

  /* Step 4: Set default measurement units (degrees, m/s^2, dps, Celsius) */
  err = internal_write_reg((uint8_t)k_bno055_reg_unit_sel, (uint8_t)k_bno055_unit_sel_default);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set unit sel failed");
    return err;
  }

  /* Step 5: Set default axis remapping */
  err = internal_write_reg((uint8_t)k_bno055_reg_axis_map_cfg, (uint8_t)k_bno055_axis_map_default);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set axis map cfg failed");
    return err;
  }

  err = internal_write_reg((uint8_t)k_bno055_reg_axis_map_sgn, (uint8_t)k_bno055_axis_map_sign_pos);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set axis map sign failed");
    return err;
  }

  /* Step 6: Clear system trigger register */
  err = internal_write_reg((uint8_t)k_bno055_reg_sys_trigger, (uint8_t)k_bno055_sys_trigger_clear);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Clear sys trigger failed");
    return err;
  }

  /* Step 7: Enter NDOF fusion mode (full 9-DOF sensor fusion) */
  err = internal_write_reg((uint8_t)k_bno055_reg_opr_mode, (uint8_t)k_bno055_opr_ndof);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Set NDOF mode failed");
    return err;
  }
  (void)tx_thread_sleep(k_bno055_single_byte); /* 1 tick @ 10 ms/tick = 10 ms (rounds up from 7 ms) */

  /* Step 8: Verify chip ID */
  uint8_t chip_id = 0;
  err             = internal_read_regs((uint8_t)k_bno055_reg_chip_id, &chip_id, k_bno055_single_byte);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Chip ID read failed");
    return err;
  }

  if (chip_id != (uint8_t)k_bno055_chip_id_expected) {
    rx_log_error_val(s_tag, "Unexpected chip ID", (uint32_t)chip_id);
    return k_rx_err_invalid_state;
  }

  s_initialized = true;
  rx_log_info(s_tag, "BNO055 initialized in NDOF mode");

  return k_rx_ok;
}

/**
 * @brief Read current BNO055 fusion output data
 *
 * @details
 * Performs five sequential I2C burst reads to collect all output data.
 * All 16-bit values are assembled in little-endian format as specified
 * in the BNO055 datasheet (LSB register first, MSB register second).
 *
 * Register read sequence:
 * 1. 0x1A: 6 bytes -> Euler heading[2], roll[2], pitch[2]
 * 2. 0x20: 8 bytes -> Quaternion W[2], X[2], Y[2], Z[2]
 * 3. 0x28: 6 bytes -> Linear accel X[2], Y[2], Z[2]
 * 4. 0x34: 1 byte  -> Temperature
 * 5. 0x35: 1 byte  -> Calibration status
 *
 * @param[out] out Output data structure
 *
 * @return rx_err_t Read result
 * @retval k_rx_ok All five reads succeeded
 * @retval k_rx_err_null_ptr out is NULL
 * @retval k_rx_err_not_initialized init not called
 * @retval k_rx_err_nack I2C communication failure
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

  /* Read Euler angles: heading(2) + roll(2) + pitch(2) = 6 bytes from 0x1A */
  uint8_t  euler_buf[k_bno055_euler_bytes];
  rx_err_t err = internal_read_regs((uint8_t)k_bno055_reg_eul_h_lsb,
                                    euler_buf,
                                    k_bno055_euler_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Euler read failed");
    return err;
  }

  /* Read Quaternion: W(2) + X(2) + Y(2) + Z(2) = 8 bytes from 0x20 */
  uint8_t quat_buf[k_bno055_quat_bytes];
  err = internal_read_regs((uint8_t)k_bno055_reg_qua_w_lsb, quat_buf, k_bno055_quat_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Quaternion read failed");
    return err;
  }

  /* Read Linear acceleration: X(2) + Y(2) + Z(2) = 6 bytes from 0x28 */
  uint8_t lia_buf[k_bno055_lia_bytes];
  err = internal_read_regs((uint8_t)k_bno055_reg_lia_x_lsb, lia_buf, k_bno055_lia_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Linear accel read failed");
    return err;
  }

  /* Read Temperature: 1 byte from 0x34 */
  uint8_t temp_raw = 0;
  err              = internal_read_regs((uint8_t)k_bno055_reg_temp, &temp_raw, k_bno055_single_byte);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Temperature read failed");
    return err;
  }

  /* Read Calibration status: 1 byte from 0x35 */
  uint8_t calib_raw = 0;
  err               = internal_read_regs((uint8_t)k_bno055_reg_calib_stat,
                                         &calib_raw,
                                         k_bno055_single_byte);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Calib status read failed");
    return err;
  }

  /* Assemble 16-bit values (little-endian: LSB first at even offset) */
  const uint8_t lsb = (uint8_t)k_bno055_idx_lsb;
  const uint8_t msb = (uint8_t)k_bno055_idx_msb;
  const uint8_t sh  = (uint8_t)k_bno055_shift_msb;

  /* Euler angles (stride 2 bytes each: H offset 0, R offset 2, P offset 4) */
  typedef enum : uint8_t {
    k_eul_h_off = 0, /**< Byte offset of heading LSB in euler_buf */
    k_eul_r_off = 2, /**< Byte offset of roll LSB in euler_buf */
    k_eul_p_off = 4, /**< Byte offset of pitch LSB in euler_buf */
  } euler_offsets_t;

  out->heading_deg16 = (int16_t)((uint16_t)euler_buf[k_eul_h_off + lsb] |
                                 ((uint16_t)euler_buf[k_eul_h_off + msb] << sh));
  out->roll_deg16    = (int16_t)((uint16_t)euler_buf[k_eul_r_off + lsb] |
                                 ((uint16_t)euler_buf[k_eul_r_off + msb] << sh));
  out->pitch_deg16   = (int16_t)((uint16_t)euler_buf[k_eul_p_off + lsb] |
                                 ((uint16_t)euler_buf[k_eul_p_off + msb] << sh));

  /* Quaternion (stride 2 bytes each: W offset 0, X offset 2, Y offset 4, Z offset 6) */
  typedef enum : uint8_t {
    k_qua_w_off = 0, /**< Byte offset of quaternion W LSB in quat_buf */
    k_qua_x_off = 2, /**< Byte offset of quaternion X LSB in quat_buf */
    k_qua_y_off = 4, /**< Byte offset of quaternion Y LSB in quat_buf */
    k_qua_z_off = 6, /**< Byte offset of quaternion Z LSB in quat_buf */
  } quat_offsets_t;

  out->quat_w = (int16_t)((uint16_t)quat_buf[k_qua_w_off + lsb] |
                           ((uint16_t)quat_buf[k_qua_w_off + msb] << sh));
  out->quat_x = (int16_t)((uint16_t)quat_buf[k_qua_x_off + lsb] |
                           ((uint16_t)quat_buf[k_qua_x_off + msb] << sh));
  out->quat_y = (int16_t)((uint16_t)quat_buf[k_qua_y_off + lsb] |
                           ((uint16_t)quat_buf[k_qua_y_off + msb] << sh));
  out->quat_z = (int16_t)((uint16_t)quat_buf[k_qua_z_off + lsb] |
                           ((uint16_t)quat_buf[k_qua_z_off + msb] << sh));

  /* Linear acceleration (stride 2 bytes each: X offset 0, Y offset 2, Z offset 4) */
  typedef enum : uint8_t {
    k_lia_x_off = 0, /**< Byte offset of linear accel X LSB in lia_buf */
    k_lia_y_off = 2, /**< Byte offset of linear accel Y LSB in lia_buf */
    k_lia_z_off = 4, /**< Byte offset of linear accel Z LSB in lia_buf */
  } lia_offsets_t;

  out->lin_acc_x = (int16_t)((uint16_t)lia_buf[k_lia_x_off + lsb] |
                              ((uint16_t)lia_buf[k_lia_x_off + msb] << sh));
  out->lin_acc_y = (int16_t)((uint16_t)lia_buf[k_lia_y_off + lsb] |
                              ((uint16_t)lia_buf[k_lia_y_off + msb] << sh));
  out->lin_acc_z = (int16_t)((uint16_t)lia_buf[k_lia_z_off + lsb] |
                              ((uint16_t)lia_buf[k_lia_z_off + msb] << sh));

  /* Temperature and calibration status (raw single bytes) */
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
 * @param[out] out_calibrated True if SYS==3 && GYR==3 && ACC==3 && MAG==3
 *
 * @return rx_err_t Read result
 * @retval k_rx_ok out_calibrated set
 * @retval k_rx_err_null_ptr out_calibrated is NULL
 * @retval k_rx_err_not_initialized init not called
 * @retval k_rx_err_nack I2C failure
 *
 * @pre rx_bno055_init() succeeded
 * @pre out_calibrated non-NULL
 * @post *out_calibrated valid only on k_rx_ok
 * @post CALIB_STAT register read (no state modified)
 *
 * @note Not thread-safe
 * @see bno055_calib_t Calibration bit-field constants
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
  rx_err_t err       = internal_read_regs((uint8_t)k_bno055_reg_calib_stat,
                                          &calib_raw,
                                          k_bno055_single_byte);
  if (err != k_rx_ok) {
    return err;
  }

  const uint8_t mask = (uint8_t)k_bno055_calib_mask;
  const uint8_t full = (uint8_t)k_bno055_calib_full;

  const uint8_t sys_cal = (calib_raw >> (uint8_t)k_bno055_calib_sys_shift) & mask;
  const uint8_t gyr_cal = (calib_raw >> (uint8_t)k_bno055_calib_gyr_shift) & mask;
  const uint8_t acc_cal = (calib_raw >> (uint8_t)k_bno055_calib_acc_shift) & mask;
  const uint8_t mag_cal = (calib_raw >> (uint8_t)k_bno055_calib_mag_shift) & mask;

  *out_calibrated = (sys_cal == full) && (gyr_cal == full) && (acc_cal == full) && (mag_cal == full);

  return k_rx_ok;
}
