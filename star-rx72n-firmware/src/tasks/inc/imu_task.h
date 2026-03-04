/* src/tasks/inc/imu_task.h */

/**
 * @file imu_task.h
 * @brief IMU Task - BNO055 + BMP280 Sensor Polling at 20 Hz
 *
 * @details
 * # Overview
 *
 * Declares the IMU task responsible for reading the BNO055 9-DOF absolute
 * orientation sensor and the BMP280 barometric pressure sensor at 20 Hz
 * (50 ms period) and publishing results to shared_data.
 *
 * # Hardware
 *
 * Both sensors share the RIIC1 I2C bus (SCL1=P2.1, SDA1=P2.0):
 * - **BNO055**: 9-DOF IMU @ I2C address 0x28, bus name "i2c1"
 * - **BMP280**: Barometric pressure @ I2C address 0x76, bus name "i2c1"
 *
 * # Task Configuration
 *
 * | Parameter | Value |
 * |-----------|-------|
 * | Priority | k_imu_task_priority = 13 (between obstacle detect and temp sensor) |
 * | Stack | k_imu_task_stack_size_bytes = 2048 bytes |
 * | Period | k_imu_task_period_ms = 50 ms (20 Hz) |
 * | IWDT timeout | k_imu_task_iwdt_timeout_ms = 150 ms (3x period) |
 *
 * # Task Lifecycle
 *
 * 1. Initialize BNO055 (rx_bno055_init) - blocks ~700 ms for POR
 * 2. Initialize BMP280 (rx_bmp280_init) - blocks ~2 ms for calib read
 * 3. Enter 50 ms periodic polling loop:
 *    a. Read BNO055 -> populate imu_state_t -> shared_data_update_imu()
 *    b. Read BMP280 -> populate baro_state_t -> shared_data_update_baro()
 *    c. Feed IWDT heartbeat
 *    d. Sleep remainder of 50 ms period
 *
 * @see imu_task.c Implementation
 * @see rx_bno055.h BNO055 driver
 * @see rx_bmp280.h BMP280 driver
 * @see shared_data.h IMU and baro state types
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 3**: [PASS] No dynamic allocation; static stack only
 * - **Rule 5**: [PASS] 2+ preconditions in imu_task_create()
 *
 * @warning rx_bno055_init() blocks for ~700 ms during power-on reset sequence.
 *          This task may delay system startup. Other tasks at lower priority
 *          will be blocked until this initialization completes.
 * @invariant The IMU task runs at 20 Hz (50 ms period = k_imu_task_period_ticks ticks)
 *            once initialization is complete.
 *
 * @author STAR Team
 * @date 2026-03-04
 * @version 1.0.0
 * @copyright MIT License
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

/**
 * @enum imu_task_priority_t
 * @brief ThreadX priority level for the IMU task
 *
 * @details
 * Priority 13 sits between obstacle detect (12) and temp sensor (15).
 * IMU data is needed by telemetry but is not as time-critical as obstacle avoidance.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_task_priority = 13U, /**< ThreadX priority for IMU task (higher number = lower priority) */
} imu_task_priority_t;

/**
 * @enum imu_task_timing_t
 * @brief Period, tick count, and IWDT timeout constants for the IMU task
 *
 * @details
 * The IMU task polls sensors at 20 Hz (50 ms period, 5 ticks at 100 Hz tick rate).
 * The IWDT heartbeat timeout is 3x the period to allow for sensor I2C latency.
 *
 * k_imu_task_period_ticks is used directly in tx_thread_sleep() to sleep for
 * exactly one 50 ms sampling period (5 ticks x 10 ms/tick = 50 ms).
 *
 * @invariant k_imu_task_period_ticks > 0 (task loop must have a finite sleep period)
 * @invariant k_imu_task_iwdt_timeout_ms >= 3 * k_imu_task_period_ms (3x safety margin)
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_imu_task_period_ms = 50U, /**< IMU sampling period in milliseconds (20 Hz) */
  k_imu_task_period_ticks =
    5U, /**< IMU sampling period in RTOS ticks (5 ticks x 10 ms/tick = 50 ms at 100 Hz) */
  k_imu_task_iwdt_timeout_ms = 150U, /**< IWDT heartbeat timeout in milliseconds (3x period) */
} imu_task_timing_t;

/**
 * @enum imu_task_stack_t
 * @brief Stack size constant for the IMU task thread
 *
 * @details
 * 2048 bytes is sufficient for BNO055 init sequence (12 register writes),
 * BMP280 calibration burst read (24-byte buffer), and periodic read buffers.
 *
 * @invariant k_imu_task_stack_size_bytes >= 2048U (minimum for BNO055 init, BMP280 calibration, and periodic read buffers)
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_imu_task_stack_size_bytes = 2048U, /**< Stack size in bytes for IMU task thread */
} imu_task_stack_t;

/**
 * @brief Create and start the IMU sensor task
 *
 * @details
 * Creates the ImuTask ThreadX task for BNO055 + BMP280 sensor polling.
 * This task initializes both sensors during its startup phase (which may
 * take ~700 ms for BNO055 POR) and then polls at 20 Hz.
 *
 * **Task Configuration:**
 * - Priority: k_imu_task_priority = 13 (lower than obstacle detect at 12, higher than temp at 15)
 * - Stack: k_imu_task_stack_size_bytes = 2048 bytes (sufficient for BNO055 + BMP280 init sequences)
 * - Period: k_imu_task_period_ms = 50 ms (20 Hz IMU and baro updates)
 * - IWDT timeout: k_imu_task_iwdt_timeout_ms = 150 ms (3x period)
 *
 * **Sensor Initialization (inside task):**
 * - rx_bno055_init(): ~700 ms (BNO055 POR + NDOF mode setup)
 * - rx_bmp280_init(): ~2 ms (calibration burst read)
 * - Both initialized before entering the periodic poll loop
 * - Sensor failures logged but do not block task startup
 *
 * **Data Flow:**
 * - BNO055 read -> imu_state_t -> shared_data_update_imu()
 * - BMP280 read -> baro_state_t -> shared_data_update_baro()
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully, will run after scheduler starts
 * @retval k_rx_err_rtos_error ThreadX task creation failed
 *
 * @pre ThreadX kernel initialized; task created in tx_application_define and will run after the scheduler starts (tx_kernel_enter)
 * @pre "i2c1" bus registered in bus manager via main.c
 * @pre shared_data_init() called (mutexes created)
 * @pre hardware_init() completed (RIIC1 initialized at 400 kHz)
 *
 * @post ImuTask created and scheduled for execution
 * @post Sensor initialization will occur inside task on first run
 * @post imu_state_t and baro_state_t updated at 20 Hz once sensors ready
 *
 * @warning Sensor initialization occurs inside the task and will delay the first
 *          data update by approximately 702 ms total (~700 ms for rx_bno055_init
 *          plus ~2 ms for rx_bmp280_init). Other tasks are not blocked; the IMU
 *          task itself is blocked during its own init sequence.
 *
 * @note Call from tx_application_define() in main.c after bus registration
 * @note Task initializes sensors internally; no pre-initialization required
 * @note BNO055 POR delay (~700 ms) occurs inside the task, not in main
 *
 * @par Example usage from tx_application_define:
 * @code{.c}
 * void tx_application_define(void* first_unused_memory)
 * {
 *     (void)first_unused_memory;
 *
 *     // Initialize shared data (mutexes) before creating tasks
 *     rx_err_t err = shared_data_init();
 *     RX_ASSERT(err == k_rx_ok, "shared_data_init must succeed");
 *
 *     // Register I2C buses for IMU sensors (must be done before imu_task_create)
 *     // (bus registration code omitted - see main.c)
 *
 *     // Create the IMU task (sensor init happens inside the task)
 *     err = imu_task_create();
 *     RX_ASSERT(err == k_rx_ok, "imu_task_create must succeed");
 *
 *     // IMU task will initialize BNO055 + BMP280 on first run (~702 ms)
 *     // and then publish imu_state_t / baro_state_t at 20 Hz
 * }
 * @endcode
 *
 * @see imu_task.c Full implementation
 * @see rx_bno055_init() BNO055 initialization (called inside task)
 * @see rx_bmp280_init() BMP280 initialization (called inside task)
 * @see shared_data.h imu_state_t and baro_state_t definitions
 * @see main.c Task creation call site
 *
 * @since Version 1.0.0
 */
rx_err_t imu_task_create(void);
