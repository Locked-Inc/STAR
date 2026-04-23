/**
 * @file serial_bringup_task.h
 * @brief Temporary ASCII Line-Protocol Bring-Up Task for SLAM MVP
 *
 * @details
 * Declares serial_bringup_task_create(). The implementation owns SCI9
 * (the Cypress CY7C65213 USB-UART bridge at /dev/ttyACM0 on the Pi5 host)
 * and implements a plain ASCII line protocol for the SLAM MVP demo:
 *
 * Pi5 -> MCU: "V <fl> <fr> <bl> <br>\n"  (four float m/s values)
 * MCU -> Pi5: "E <fl> <fr> <bl> <br> <ms>\n"
 *             (four int16 encoder ticks, then uint32 ThreadX ms tick)
 * MCU -> Pi5: "M <d0> <d1> <d2> <d3> <f0> <f1> <f2> <f3> <ms>\n"
 *             (per-motor duty in tenths of a percent (int16), then
 *              per-motor DRV8263 fault byte (uint8), firmware order
 *              FL FR BR BL, then uint32 ThreadX ms tick)
 * MCU -> Pi5: "I <qw> <qx> <qy> <qz> <roll> <pitch> <heading>
 *                <gx> <gy> <gz> <ax> <ay> <az> <ms>\n"
 *             (raw int16 BNO055-scaled quaternion, Euler angles,
 *              gyro rates, and linear accel -- host applies the
 *              k_imu_scale_* divisors -- then uint32 ThreadX ms tick)
 *
 * This task is a temporary substitute for the framed nanopb/CRC-32/HARQ
 * stack implemented in comm_task / telemetry_task. Those remain in the
 * source tree and are re-enabled by commenting out
 * serial_bringup_task_create() in main.c and restoring the comm / telemetry
 * task_create() calls. No backward-compatibility shim exists between the
 * two paths -- they are mutually exclusive.
 *
 * The task fires 4 safety events that the framed path did NOT require
 * because HARQ+session provided them implicitly:
 *
 * 1. ASCII parse error -> line silently dropped with a warn log.
 * 2. Line longer than 80 bytes -> dropped, ring buffer reset, warn logged.
 * 3. No "V ..." line seen for 200 ms -> push zero motor_command_t with
 *    valid=false (motor_control_task already maps valid=false -> 0% duty).
 * 4. Telemetry emitted at 50 Hz inside the task loop (100 Hz outer loop).
 *
 * @see serial_bringup_task.c Implementation
 * @see motor_control_task.c Consumer of motor_command_t shared state
 * @see comm_task.c Framed alternative (disabled during MVP)
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the ASCII bring-up task
 *
 * @details
 * Creates the ThreadX thread "SerialBU" that owns SCI9 RX/TX and performs
 * the ASCII line protocol described in the file-level doc block. Motor
 * commands parsed from "V ..." lines are pushed into shared_data via
 * shared_data_set_motor_command() so motor_control_task can consume them
 * exactly like it would a framed command. Encoder telemetry is emitted
 * directly as ASCII from this task; telemetry_task is NOT used in MVP
 * mode.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                     Task created successfully
 * @retval k_rx_err_invalid_state      Task already created
 * @retval k_rx_err_rtos_thread_create tx_thread_create() returned non-success
 *
 * @pre ThreadX kernel running (tx_application_define already returning)
 * @pre uart_debug_init() has initialized SCI9 (called from main.c pre-RTOS)
 * @pre shared_data_init() has succeeded
 *
 * @post SerialBU thread created at priority 5 with 4 KiB stack, auto-started
 * @post Task will start draining SCI9 RX on its first schedule
 *
 * @note Not thread-safe. Call from tx_application_define() only.
 * @note Replaces comm_task_create() + telemetry_task_create() for MVP only.
 *
 * @see serial_bringup_task.c Implementation details
 * @see main.c internal_create_system_tasks() Task wiring site
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t serial_bringup_task_create(void);
