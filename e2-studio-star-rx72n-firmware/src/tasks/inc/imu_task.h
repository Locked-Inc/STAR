/* src/tasks/inc/imu_task.h */

/**
 * @file imu_task.h
 * @brief IMU Task - BNO055 9-DOF IMU and BMP280 Barometric Pressure Sensor
 *
 * @details
 * Declares the IMU task that continuously reads orientation, acceleration,
 * gyroscope data from the BNO055 and pressure/temperature/altitude from the
 * BMP280. Both sensors share the SCI2 Simple IIC (SIIC) bus on P52/P50.
 *
 * **Sensor Configuration:**
 * - BNO055: NDOF fusion mode, 0x28 (COM3=GND), 100 Hz update rate
 * - BMP280: Normal mode, osrs_t×1/osrs_p×4/IIR=16, 0x76 (SDO=GND), ~4 Hz update rate
 *
 * **Task Scheduling:**
 * - Period: 10 ms (100 Hz) via tx_thread_sleep(1)
 * - BNO055: Every loop iteration (100 Hz)
 * - BMP280: Every 25th iteration (~4 Hz, matching sensor ODR ≈ 3.82 Hz)
 *
 * **Data Flow:**
 * imu_task → shared_data_update_imu()       → shared_data.imu_state
 * imu_task → shared_data_update_barometer() → shared_data.barometer_state
 *
 * @see imu_task.c Implementation
 * @see rx_bno055.h BNO055 driver
 * @see rx_bmp280.h BMP280 driver
 * @see shared_data.h IMU and barometer state definitions
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the IMU sensor task
 *
 * @details
 * Creates the ImuTask ThreadX thread. Initializes BNO055 and BMP280 sensors
 * over SCI2 SIIC bus before entering the main read loop.
 *
 * **Task Configuration:**
 * - Priority: 13 (between obstacle=12 and BMS/temp=15)
 * - Stack: 2048 bytes
 * - Period: 10 ms (100 Hz for IMU, 4 Hz for barometer)
 * - Auto-start: Enabled
 *
 * **Initialization sequence (inside task):**
 * 1. sci_iic_init() already called by hardware_init()
 * 2. rx_bno055_init() — verifies CHIP_ID, enters NDOF mode
 * 3. rx_bmp280_init() — verifies CHIP_ID, reads trim, enters normal mode
 * 4. Main loop at 100 Hz begins
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created and running
 * @retval k_rx_err_rtos_error ThreadX task creation failed
 *
 * @pre ThreadX kernel running (tx_kernel_enter() called)
 * @pre sci_iic_init(k_imu_iic_channel) called by hardware_init()
 * @pre shared_data_init() called
 *
 * @post ImuTask thread created and ready to run
 * @post IMU and barometer data will appear in shared_data once sensors init
 *
 * @note Call from tx_application_define() after shared_data_init()
 * @note Sensor init failures are logged but do not stop the task
 *
 * @see imu_task.c Task implementation
 * @see tx_application_define() Caller
 *
 * @since Version 1.0.0
 */
rx_err_t imu_task_create(void);
