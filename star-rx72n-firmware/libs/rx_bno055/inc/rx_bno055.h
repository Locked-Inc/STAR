/**
 * @file rx_bno055.h
 * @brief BNO055 9-DOF Absolute Orientation Sensor Driver API
 *
 * @details
 * # Overview
 *
 * Platform-independent driver for the Bosch BNO055 intelligent 9-axis absolute
 * orientation sensor. The BNO055 integrates accelerometer, gyroscope, and
 * geomagnetic sensor with an on-chip ARM Cortex-M0 that performs hardware sensor
 * fusion to produce absolute orientation data (Euler angles, quaternions, linear
 * acceleration).
 *
 * This driver abstracts the BNO055 initialization sequence and data reading into
 * three simple functions: init, read, and calibration check.
 *
 * # Key Features
 *
 * - **Absolute orientation**: Euler angles (heading, roll, pitch) referenced to
 *   magnetic north and gravity
 * - **Quaternion output**: Unit quaternion for rotation-matrix computation
 * - **Linear acceleration**: Gravity-compensated acceleration in m/s^2
 * - **Calibration status**: Per-subsystem calibration quality (0-3)
 * - **Temperature**: On-chip temperature sensor
 *
 * # Hardware Configuration (STAR Project)
 *
 * - **Module**: DFRobot BNO055 breakout
 * - **I2C Address**: 0x28 (COM3/ADR pulled LOW)
 * - **I2C Bus**: RIIC1 (SCL1=P2.1, SDA1=P2.0)
 * - **Bus name in bus manager**: "i2c1_imu"
 * - **Reset pin**: P8.3 (active-low, driven HIGH by hardware_init)
 *
 * # Operating Mode
 *
 * The driver initializes the sensor in NDOF (Nine Degrees of Freedom) mode with:
 * - All three sensors active (accelerometer, gyroscope, magnetometer)
 * - Hardware sensor fusion running on the BNO055's ARM Cortex-M0
 * - Default measurement units: degrees, m/s^2, dps, Celsius
 *
 * # Initialization Sequence
 *
 * Before calling rx_bno055_init(), the caller (imu_task.c) performs a hardware
 * reset via P83 (IMU RST, active-low): assert LOW for >= 10 ms, then deassert
 * HIGH and wait 650 ms for the BNO055 POR sequence to complete. See
 * internal_imu_hardware_reset() in imu_task.c.
 *
 * The rx_bno055_init() function then performs the following steps:
 *
 * 1. Software reset (SYS_TRIGGER = 0x20), wait 650 ms (belt-and-suspenders)
 * 2. Set CONFIG mode (OPR_MODE = 0x00), wait 19 ms
 * 3. Set normal power mode (PWR_MODE = 0x00)
 * 4. Set default units (UNIT_SEL = 0x00)
 * 5. Set default axis map (AXIS_MAP_CONFIG = 0x24, AXIS_MAP_SIGN = 0x00)
 * 6. Clear system trigger (SYS_TRIGGER = 0x00)
 * 7. Set NDOF fusion mode (OPR_MODE = 0x0C), wait 7 ms
 * 8. Verify chip ID == 0xA0
 *
 * # Output Data Scales
 *
 * | Field | Raw Type | Scale | Unit |
 * |-------|----------|-------|------|
 * | heading_deg16 | int16_t | / 16 | degrees |
 * | roll_deg16 | int16_t | / 16 | degrees |
 * | pitch_deg16 | int16_t | / 16 | degrees |
 * | quat_w/x/y/z | int16_t | / 16384 | dimensionless |
 * | lin_acc_x/y/z | int16_t | / 100 | m/s^2 |
 * | temp_degc | int8_t | 1.0 | deg C |
 * | calib_stat | uint8_t | raw | - |
 *
 * @see rx_bno055_regs.h Register map and scale constants
 * @see rx_bno055.c Implementation
 * @see rx_bus_i2c.h I2C bus abstraction used
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1** (No goto): [PASS] No goto, recursion, or setjmp
 * - **Rule 3** (No heap): [PASS] Zero dynamic allocation
 * - **Rule 5** (Assertions): [PASS] 2+ preconditions per function
 * - **Rule 7** (Return checks): [PASS] All I2C returns validated
 * - **Rule 8** (Preprocessor): [PASS] C23 typed enums, no magic numbers
 * - **Rule 9** (Pointers): [WARN] Function pointers for DIP (bus manager)
 *
 * @par SOLID Principles:
 * - **S**: Module handles ONLY BNO055 sensor operations
 * - **O**: Extensible via bus manager injection
 * - **L**: Any rx_bus_manager_t* is interchangeable
 * - **I**: Focused 3-function API
 * - **D**: Depends on rx_bus_manager_t abstraction, not RIIC directly
 *
 * @author STAR Team
 * @date 2026-03-04
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

/* =============================================================================
 * Data Types
 * =============================================================================
 */

/**
 * @enum bno055_mode_t
 * @brief BNO055 driver operating mode selection
 *
 * @details
 * Selects whether the BNO055 INT pin is configured to assert on each new data
 * sample (interrupt-driven mode) or left unconfigured (polling mode).
 *
 * In interrupt-driven mode, rx_bno055_init() programs five Page 1 registers
 * after entering NDOF mode:
 * 1. PAGE_ID = 1 (switch to Page 1)
 * 2. ACC_AM_THRES = 0x01 (minimum threshold ~3.9 mg, fires at data rate)
 * 3. ACC_INT_SETTINGS = 0x07 (enable any-motion on X+Y+Z axes)
 * 4. INT_EN = 0x20 (enable accelerometer any-motion interrupt source)
 * 5. PAGE_ID = 0 (restore Page 0 for normal sensor reads)
 *
 * In polling mode, none of these writes occur and the INT pin never asserts.
 *
 * @see bno055_config_t Configuration struct passed to rx_bno055_init()
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_mode_poll = 0U, /**< Polling mode: INT pin not configured; caller polls at fixed rate */
  k_bno055_mode_interrupt = 1U, /**< Interrupt mode: configure BNO055 INT pin via ACC any-motion */
} bno055_mode_t;

/**
 * @struct bno055_config_t
 * @brief BNO055 driver configuration passed to rx_bno055_init()
 *
 * @details
 * Allows the caller to select polling or interrupt-driven mode. In interrupt
 * mode, rx_bno055_init() programs the BNO055 Page 1 interrupt engine registers
 * after chip ID verification so that the INT pin asserts on each new sample.
 *
 * @invariant mode must be a valid bno055_mode_t value
 *
 * @code
 * // Polling mode (no INT pin activity):
 * const bno055_config_t cfg = {.mode = k_bno055_mode_poll};
 * rx_bno055_init(&manager, &cfg);
 *
 * // Interrupt mode (BNO055 INT -> RX72N IRQ12):
 * const bno055_config_t cfg = {.mode = k_bno055_mode_interrupt};
 * rx_bno055_init(&manager, &cfg);
 * @endcode
 *
 * @see bno055_mode_t Mode selection enumeration
 * @see rx_bno055_init() Consumes this configuration struct
 *
 * @since Version 1.0.0
 */
typedef struct {
  bno055_mode_t mode; /**< Operating mode: k_bno055_mode_poll or k_bno055_mode_interrupt */
} bno055_config_t;

/**
 * @enum bno055_calib_shift_t
 * @brief BNO055 CALIB_STAT register bit-shift positions for each subsystem
 *
 * @details
 * CALIB_STAT register (0x35) contains four 2-bit calibration level fields.
 * Right-shift the raw byte by the appropriate constant, then AND with
 * k_bno055_calib_level_mask to extract a value in [0, 3].
 *
 * @code
 * uint8_t sys_lvl = (calib_stat >> k_bno055_calib_sys_shift) & k_bno055_calib_level_mask;
 * uint8_t mag_lvl = (calib_stat >> k_bno055_calib_mag_shift) & k_bno055_calib_level_mask;
 * @endcode
 *
 * @see bno055_calib_mask_t Mask and full-calibration sentinel values
 * @see rx_bno055_is_calibrated() Checks all four fields simultaneously
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_calib_mag_shift = 0U, /**< Bit shift for magnetometer calibration level (bits 1:0) */
  k_bno055_calib_acc_shift = 2U, /**< Bit shift for accelerometer calibration level (bits 3:2) */
  k_bno055_calib_gyr_shift = 4U, /**< Bit shift for gyroscope calibration level (bits 5:4) */
  k_bno055_calib_sys_shift = 6U, /**< Bit shift for system calibration level (bits 7:6) */
} bno055_calib_shift_t;

/**
 * @enum bno055_calib_mask_t
 * @brief BNO055 calibration level mask and fully-calibrated sentinel
 *
 * @details
 * After shifting a CALIB_STAT field into the LSB position, AND with
 * k_bno055_calib_level_mask to isolate the 2-bit value. Compare against
 * k_bno055_calib_full (== 3) to check for full calibration.
 *
 * @see bno055_calib_shift_t Shift constants for each subsystem
 * @see rx_bno055_is_calibrated() Uses both mask and full sentinel
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_bno055_calib_level_mask = 0x03U, /**< 2-bit mask for a single calibration level (0-3) */
  k_bno055_calib_full       = 0x03U, /**< Fully calibrated level */
} bno055_calib_mask_t;

/**
 * @enum bno055_scale_t
 * @brief BNO055 output data scale factors (divisors for unit conversion)
 *
 * @details
 * Scale factors to convert raw 16-bit integer fields in bno055_data_t to
 * physical units. Divide the raw register value by the corresponding constant.
 *
 * @code
 * float heading_deg = (float)data.heading_deg16 / (float)k_bno055_scale_heading;
 * float quat_w_f    = (float)data.quat_w        / (float)k_bno055_scale_quat;
 * float acc_x_mps2  = (float)data.lin_acc_x     / (float)k_bno055_scale_acc;
 * @endcode
 *
 * @see bno055_data_t Raw integer fields that use these scales
 * @see rx_bno055_read() Populates bno055_data_t with raw scaled integers
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_bno055_scale_heading =
    16U, /**< Degrees-per-unit for heading/roll/pitch (divide by 16 for degrees) */
  k_bno055_scale_quat = 16384U, /**< Quaternion unit (2^14; divide by 16384 for normalized float) */
  k_bno055_scale_acc  = 100U,   /**< m/s^2 per unit for linear acceleration (divide by 100) */
  k_bno055_scale_gyro = 16U,    /**< deg/s per unit for gyro fields (divide by 16 for deg/s) */
} bno055_scale_t;

/**
 * @struct bno055_data_t
 * @brief BNO055 sensor output data (raw scaled integers)
 *
 * @details
 * All values are stored as raw 16-bit integers to avoid floating-point
 * operations in the driver. Convert to physical units using the scale factors
 * defined in bno055_scale_t.
 *
 * **Conversion formulas:**
 * @code
 * float heading_deg = (float)data.heading_deg16 / (float)k_bno055_scale_heading;
 * float roll_deg    = (float)data.roll_deg16    / (float)k_bno055_scale_heading;
 * float pitch_deg   = (float)data.pitch_deg16   / (float)k_bno055_scale_heading;
 * float quat_w_f    = (float)data.quat_w        / (float)k_bno055_scale_quat;
 * float acc_x_mps2  = (float)data.lin_acc_x     / (float)k_bno055_scale_acc;
 * @endcode
 *
 * @invariant quat_w^2 + quat_x^2 + quat_y^2 + quat_z^2 == k_bno055_scale_quat^2 (unit quaternion)
 * @invariant heading_deg16 in [0, 360 * k_bno055_scale_heading - 1] (0 to 359.9375 degrees * 16)
 * @invariant temp_degc in [-40, 85] (operating range of BNO055)
 *
 * @see bno055_scale_t Scale factor constants for unit conversion
 * @see rx_bno055_read() Populates this structure
 *
 * @since Version 1.0.0
 */
typedef struct {
  int16_t
    heading_deg16; /**< Euler heading 0-359.9375 deg (divide by k_bno055_scale_heading for degrees) */
  int16_t
    roll_deg16; /**< Euler roll -90 to +90 deg (divide by k_bno055_scale_heading for degrees) */
  int16_t
    pitch_deg16; /**< Euler pitch -180 to +180 deg (divide by k_bno055_scale_heading for degrees) */
  int16_t quat_w; /**< Quaternion W component (divide by k_bno055_scale_quat for unit quaternion) */
  int16_t quat_x; /**< Quaternion X component (divide by k_bno055_scale_quat for unit quaternion) */
  int16_t quat_y; /**< Quaternion Y component (divide by k_bno055_scale_quat for unit quaternion) */
  int16_t quat_z; /**< Quaternion Z component (divide by k_bno055_scale_quat for unit quaternion) */
  int16_t
    lin_acc_x; /**< Linear accel X in m/s^2 * k_bno055_scale_acc (divide by k_bno055_scale_acc for m/s^2) */
  int16_t
    lin_acc_y; /**< Linear accel Y in m/s^2 * k_bno055_scale_acc (divide by k_bno055_scale_acc for m/s^2) */
  int16_t
    lin_acc_z; /**< Linear accel Z in m/s^2 * k_bno055_scale_acc (divide by k_bno055_scale_acc for m/s^2) */
  int16_t gyro_x_dps16; /**< Gyroscope X in dps * k_bno055_scale_gyro (divide by 16 for deg/s) */
  int16_t gyro_y_dps16; /**< Gyroscope Y in dps * k_bno055_scale_gyro (divide by 16 for deg/s) */
  int16_t gyro_z_dps16; /**< Gyroscope Z in dps * k_bno055_scale_gyro (divide by 16 for deg/s) */
  int8_t  temp_degc;    /**< On-chip temperature in degrees Celsius (1 deg C per LSB) */
  uint8_t calib_stat;   /**< Raw CALIB_STAT byte: SYS[7:6] GYR[5:4] ACC[3:2] MAG[1:0] */
} bno055_data_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BNO055 sensor and configure for NDOF fusion mode
 *
 * @details
 * Performs the complete BNO055 initialization sequence:
 * 1. Software reset via SYS_TRIGGER register (wait 650 ms for POR)
 * 2. Enter CONFIG mode for register writes (wait 19 ms)
 * 3. Configure power mode to Normal
 * 4. Set measurement units to default (degrees, m/s^2, dps, Celsius)
 * 5. Set default axis mapping (standard orientation)
 * 6. Clear system trigger register
 * 7. Enter NDOF fusion mode (wait 7 ms for mode transition)
 * 8. Verify CHIP_ID register reads 0xA0
 * 9. (If config->mode == k_bno055_mode_interrupt) Configure Page 1 interrupt
 *    engine so the INT pin asserts on each new accelerometer sample
 *
 * The function stores the bus manager pointer in a module-static variable
 * for use by subsequent rx_bno055_read() calls. This design assumes a single
 * BNO055 instance (no multi-instance support needed for STAR hardware).
 *
 * @param[in] manager Pointer to initialized bus manager.
 *                    Must have "i2c1_imu" bus registered and initialized.
 *                    Must not be NULL.
 * @param[in] config  Pointer to driver configuration selecting poll vs interrupt
 *                    mode. Must not be NULL.
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Sensor initialized, NDOF mode active, ready to read
 * @retval k_rx_err_null_ptr manager or config is NULL
 * @retval k_rx_err_nack I2C NACK - device not found (check address/wiring)
 * @retval k_rx_err_invalid_state CHIP_ID mismatch (wrong device or I2C issue)
 * @retval k_rx_err_timeout I2C transaction timeout
 *
 * @pre Caller must have completed hardware reset via P83 (IMU RST, active-low):
 *      assert LOW for >= 10 ms, deassert HIGH, wait >= 650 ms for POR to complete.
 *      See internal_imu_hardware_reset() in imu_task.c.
 * @pre manager must be initialized via rx_bus_manager_init()
 * @pre "i2c1_imu" bus must be registered and initialized in manager
 * @pre RIIC1 hardware initialized by hardware_init() (riic_init)
 * @pre BNO055 powered and connected to I2C bus
 *
 * @post Sensor in NDOF fusion mode
 * @post Internal s_manager pointer stored for subsequent reads
 * @post s_initialized flag set to true on success
 * @post Sensor actively running sensor fusion (accelerometer + gyro + mag)
 * @post INT pin configured to assert on data ready if config->mode == k_bno055_mode_interrupt
 *
 * @note Not thread-safe. Call from single-threaded initialization context.
 * @note Blocks for ~700 ms total due to required startup delays.
 * @warning Do not call if hardware_init() has not been called first.
 *
 * @par Performance:
 * - Execution time: ~700 ms (dominated by 650 ms POR delay)
 * - I2C transactions: ~12 writes + 1 read (polling); ~17 writes + 1 read (interrupt mode)
 *
 * @see rx_bno055_read() Read sensor data after initialization
 * @see bno055_mode_t Mode selection (poll vs interrupt)
 * @see bno055_config_t Configuration struct
 * @see bno055_delay_ms_t Required timing constants
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bno055_init(rx_bus_manager_t* manager, const bno055_config_t* config);

/**
 * @brief Read current BNO055 fusion output data
 *
 * @details
 * Performs six I2C read transactions to collect all fusion output data:
 * 1. Read 6 bytes from 0x14: Gyroscope X(2) + Y(2) + Z(2)
 * 2. Read 6 bytes from 0x1A: Euler heading(2) + roll(2) + pitch(2)
 * 3. Read 8 bytes from 0x20: Quaternion W(2) + X(2) + Y(2) + Z(2)
 * 4. Read 6 bytes from 0x28: Linear accel X(2) + Y(2) + Z(2)
 * 5. Read 1 byte from 0x34: Temperature
 * 6. Read 1 byte from 0x35: Calibration status
 *
 * All multi-byte values are assembled in little-endian format (LSB | (MSB << 8)).
 *
 * @param[out] out Pointer to output data structure. Must not be NULL.
 *                 Receives all sensor fusion data on success.
 *
 * @return rx_err_t Read result
 * @retval k_rx_ok Data successfully read into *out
 * @retval k_rx_err_null_ptr out is NULL
 * @retval k_rx_err_not_initialized rx_bno055_init() not called yet
 * @retval k_rx_err_nack I2C NACK during read (sensor not responding)
 * @retval k_rx_err_timeout I2C read timeout
 *
 * @pre rx_bno055_init() called successfully
 * @pre out must point to valid bno055_data_t storage
 *
 * @post out->gyro_x/y/z_dps16 contains gyroscope angular rate (divide by 16 for deg/s)
 * @post out->heading_deg16 contains Euler heading in 1/16 degree units
 * @post out->quat_w/x/y/z contains unit quaternion (divide by 16384)
 * @post out->lin_acc_x/y/z contains linear acceleration (divide by 100 for m/s^2)
 * @post out->calib_stat contains raw calibration status byte
 *
 * @note Not thread-safe; call from single task only
 * @note Calibration improves over time as sensor moves; check calib_stat
 *
 * @par Performance:
 * - Execution time: ~2 ms at 400 kHz I2C (6 transactions)
 * - I2C bytes transferred: 28 bytes read
 *
 * @see rx_bno055_is_calibrated() Check calibration before relying on data
 * @see bno055_scale_t Scale factor constants for unit conversion
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bno055_read(bno055_data_t* out);

/**
 * @brief Check whether all BNO055 subsystems are fully calibrated
 *
 * @details
 * Reads the CALIB_STAT register (0x35) and checks whether all four
 * calibration fields (SYS, GYR, ACC, MAG) are at level 3 (fully calibrated).
 *
 * The BNO055 calibration process:
 * - **Magnetometer**: Move sensor in figure-8 pattern
 * - **Gyroscope**: Keep sensor stationary for a few seconds
 * - **Accelerometer**: Hold in 6 different orientations
 * - **System**: All subsystems must be calibrated
 *
 * Calibration is stored in non-volatile EEPROM inside the BNO055.
 *
 * @param[out] out_calibrated Pointer to bool to receive result. Must not be NULL.
 *                            Set to true if all four subsystems at level 3.
 *
 * @return rx_err_t Read result
 * @retval k_rx_ok out_calibrated set (true or false)
 * @retval k_rx_err_null_ptr out_calibrated is NULL
 * @retval k_rx_err_not_initialized rx_bno055_init() not called
 * @retval k_rx_err_nack I2C communication failure
 * @retval k_rx_err_timeout I2C read timeout
 *
 * @pre rx_bno055_init() called successfully
 * @pre out_calibrated points to valid bool storage
 *
 * @post *out_calibrated == true only if SYS==3 && GYR==3 && ACC==3 && MAG==3
 * @post CALIB_STAT register read over I2C (no state modified)
 *
 * @note Not thread-safe
 * @note Calibration status can be monitored in real-time via bno055_data_t.calib_stat
 *
 * @see bno055_calib_shift_t Bit-shift constants for each calibration subsystem field
 * @see bno055_calib_mask_t Mask and fully-calibrated sentinel for CALIB_STAT fields
 * @see rx_bno055_read() Reads calib_stat without separate I2C transaction
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bno055_is_calibrated(bool* out_calibrated);

/**
 * @brief Clear the BNO055 INT pin by reading the INT_STA register (Page 0, 0x37)
 *
 * @details
 * Per BNO055 datasheet section 3.6, the INT output pin is asserted when an
 * enabled-and-unmasked interrupt source fires and stays asserted until
 * firmware reads the INT_STA register. Without this read, the pin latches
 * high after the first interrupt and never produces another edge, so
 * edge-triggered external IRQs on the host MCU fire exactly once.
 *
 * Call this after every sensor read (or at any other convenient cadence)
 * so the pin re-triggers on the next ACC_BSX_DRDY event.
 *
 * @return rx_err_t Operation result
 * @retval k_rx_ok INT_STA read successfully (pin cleared)
 * @retval k_rx_err_invalid_state Driver not yet initialized
 * @retval k_rx_err_timeout / k_rx_err_nack I2C failure
 *
 * @pre rx_bno055_init() completed with k_rx_ok
 * @post BNO055 INT pin is low; pending-interrupt bits in INT_STA are cleared
 *
 * @note Safe to call with no pending interrupts -- the read is idempotent.
 * @note Not thread-safe.
 */
[[nodiscard]] rx_err_t rx_bno055_clear_int(void);

#ifdef UNIT_TEST
/**
 * @brief Reset all driver static state to uninitialized (test-only)
 *
 * @details
 * Clears s_initialized and s_manager so that each unit test starts from a
 * clean, uninitialized state regardless of execution order. This function is
 * only compiled when UNIT_TEST is defined (host-side builds). It must NOT be
 * called from production code.
 *
 * @pre Driver may be in any state (initialized or not)
 * @pre Called only from test setUp() -- never from production firmware
 * @post s_initialized == false
 * @post s_manager == NULL
 *
 * @note For unit tests only; not available in firmware builds
 * @since Version 1.0.0
 */
void rx_bno055_test_reset_state(void);
#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
