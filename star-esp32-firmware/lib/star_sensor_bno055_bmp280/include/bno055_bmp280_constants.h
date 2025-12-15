/* lib/star_sensor_bno055_bmp280/include/bno055_bmp280_constants.h */

/**
 * @file bno055_bmp280_constants.h
 * @brief Constants for BNO055 + BMP280 10-DOF sensor module
 */

#ifndef BNO055_BMP280_CONSTANTS_H
#define BNO055_BMP280_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Type-safe I2C addresses
 * Using enum for compile-time constant behavior in switch statements */
enum {
  k_bno055_i2c_addr = 0x28,
  k_bmp280_i2c_addr = 0x76,
};

/* BNO055 Register Map */
typedef enum {
  k_bno055_reg_chip_id         = 0x00,
  k_bno055_reg_acc_data_x_lsb  = 0x08,
  k_bno055_reg_mag_data_x_lsb  = 0x0E,
  k_bno055_reg_gyr_data_x_lsb  = 0x14,
  k_bno055_reg_eul_heading_lsb = 0x1A,
  k_bno055_reg_qua_data_w_lsb  = 0x20,
  k_bno055_reg_lia_data_x_lsb  = 0x28,
  k_bno055_reg_grv_data_x_lsb  = 0x2E,
  k_bno055_reg_temp            = 0x34,
  k_bno055_reg_calib_stat      = 0x35,
  k_bno055_reg_opr_mode        = 0x3D,
  k_bno055_reg_pwr_mode        = 0x3E,
  k_bno055_reg_sys_trigger     = 0x3F
} bno055_register_t;

/* BNO055 Operation Modes */
typedef enum {
  k_bno055_mode_config       = 0x00,
  k_bno055_mode_acconly      = 0x01,
  k_bno055_mode_magonly      = 0x02,
  k_bno055_mode_gyronly      = 0x03,
  k_bno055_mode_accmag       = 0x04,
  k_bno055_mode_accgyro      = 0x05,
  k_bno055_mode_maggyro      = 0x06,
  k_bno055_mode_amg          = 0x07,
  k_bno055_mode_imu          = 0x08,
  k_bno055_mode_compass      = 0x09,
  k_bno055_mode_m4g          = 0x0A,
  k_bno055_mode_ndof_fmc_off = 0x0B,
  k_bno055_mode_ndof         = 0x0C
} bno055_opmode_t;

/* BNO055 Power Modes */
typedef enum {
  k_bno055_pwr_mode_normal   = 0x00,
  k_bno055_pwr_mode_lowpower = 0x01,
  k_bno055_pwr_mode_suspend  = 0x02
} bno055_pwrmode_t;

/* BMP280 Register Map */
typedef enum {
  k_bmp280_reg_calib00    = 0x88,
  k_bmp280_reg_chip_id    = 0xD0,
  k_bmp280_reg_reset      = 0xE0,
  k_bmp280_reg_status     = 0xF3,
  k_bmp280_reg_ctrl_meas  = 0xF4,
  k_bmp280_reg_config     = 0xF5,
  k_bmp280_reg_press_msb  = 0xF7,
  k_bmp280_reg_press_lsb  = 0xF8,
  k_bmp280_reg_press_xlsb = 0xF9,
  k_bmp280_reg_temp_msb   = 0xFA,
  k_bmp280_reg_temp_lsb   = 0xFB,
  k_bmp280_reg_temp_xlsb  = 0xFC
} bmp280_register_t;

/* BMP280 Configuration Constants */
typedef enum {
  k_bmp280_mode_sleep  = 0x00,
  k_bmp280_mode_forced = 0x01,
  k_bmp280_mode_normal = 0x03
} bmp280_mode_t;

typedef enum {
  k_bmp280_oversample_skip = 0x00,
  k_bmp280_oversample_x1   = 0x01,
  k_bmp280_oversample_x2   = 0x02,
  k_bmp280_oversample_x4   = 0x03,
  k_bmp280_oversample_x8   = 0x04,
  k_bmp280_oversample_x16  = 0x05
} bmp280_oversampling_t;

typedef enum {
  k_bmp280_filter_off = 0x00,
  k_bmp280_filter_x2  = 0x01,
  k_bmp280_filter_x4  = 0x02,
  k_bmp280_filter_x8  = 0x03,
  k_bmp280_filter_x16 = 0x04
} bmp280_filter_t;

/* Chip ID and System Trigger Constants
 * Using enum for compile-time constant behavior */
enum {
  k_bno055_chip_id_value   = 0xA0,
  k_bmp280_chip_id_value   = 0x58,
  k_bno055_sys_trigger_rst = 0x20,
};

#ifdef __cplusplus
}
#endif

#endif /* BNO055_BMP280_CONSTANTS_H */