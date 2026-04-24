/**
 * @file bno055.h
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

/* bno055.h -- minimal BNO055 driver layered on top of r_riic_rx (RIIC1).
 *
 * IMU mode (OPR_MODE = 0x08) gives raw accel + gyro fused at 100 Hz with no
 * orientation -- which is what robot_localization wants on the Pi side
 * (gravity-vector and quaternion are best fused on the Pi using ROS2's full
 * sensor graph). This driver does NOT touch the magnetometer.
 *
 * All I2C transactions go through the FIT module's R_RIIC_MasterSend /
 * R_RIIC_MasterReceive functions. The FIT module owns the RIIC ISR; we just
 * spin on a status flag with a generous timeout.
 *
 * Verify R_RIIC_* function signatures against
 * vendor/FITModules/r_riic_rx/r_riic_rx_if.h after fetch -- the API is
 * stable enough that this skeleton should compile, but const-qualifiers and
 * the slave-address field name have varied across versions.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool bno055_open_bus(void);                 /* call ONCE: R_RIIC_Open(RIIC1) */
bool bno055_init(void);                     /* probe chip_id, set IMU mode */

/* One-shot read: 6 floats packed into out[0..5] = accel xyz + gyro xyz.
 * Units: accel m/s^2, gyro rad/s. Returns false on I2C error. */
bool bno055_read_imu(float out_accel_mps2[3], float out_gyro_rps[3]);
