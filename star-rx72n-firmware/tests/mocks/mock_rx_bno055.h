/**
 * @file mock_rx_bno055.h
 * @brief Mock BNO055 9-DOF IMU sensor driver for IMU task unit tests
 *
 * @details
 * Provides test double for BNO055 9-DOF absolute orientation sensor.
 * Enables testing of IMU task initialization and interrupt-driven data
 * handling without actual BNO055 hardware.
 *
 * @author STAR Team
 * @date 2026-03-08
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

/* Forward declaration */
struct rx_bus_manager_s;
typedef struct rx_bus_manager_s rx_bus_manager_t;

/**
 * @struct bno055_data_t
 * @brief BNO055 sensor data output structure (mock version)
 */
typedef struct {
  int16_t heading_deg16; /**< Euler heading */
  int16_t roll_deg16;    /**< Euler roll */
  int16_t pitch_deg16;   /**< Euler pitch */
  int16_t quat_w;        /**< Quaternion W */
  int16_t quat_x;        /**< Quaternion X */
  int16_t quat_y;        /**< Quaternion Y */
  int16_t quat_z;        /**< Quaternion Z */
  int16_t lin_acc_x;     /**< Linear accel X */
  int16_t lin_acc_y;     /**< Linear accel Y */
  int16_t lin_acc_z;     /**< Linear accel Z */
  int16_t gyro_x_dps16;  /**< Gyroscope X */
  int16_t gyro_y_dps16;  /**< Gyroscope Y */
  int16_t gyro_z_dps16;  /**< Gyroscope Z */
  int8_t  temp_degc;     /**< Temperature */
  uint8_t calib_stat;    /**< Calibration status */
} bno055_data_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_bno055_reset(void);
void mock_bno055_set_init_return(rx_err_t err);
void mock_bno055_set_read_return(rx_err_t err);

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_bno055_get_init_count(void);
uint32_t mock_bno055_get_read_count(void);

/* =============================================================================
 * Mock BNO055 API (matches rx_bno055.h signatures)
 * =============================================================================
 */

rx_err_t rx_bno055_init(rx_bus_manager_t* manager);
rx_err_t rx_bno055_read(bno055_data_t* out);

#ifdef __cplusplus
}
#endif
