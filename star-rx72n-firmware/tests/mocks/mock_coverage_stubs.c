/**
 * @file mock_coverage_stubs.c
 * @brief Minimal symbol stubs for the coverage-baseline test executable.
 *
 * @details
 * Provides no-op stub implementations of firmware functions and variables
 * that are referenced by the real task/shared-data source files compiled in
 * firmware_coverage_obj but are not provided by any other mock in
 * test_coverage_compile_all.
 *
 * Every stub returns the "success" sentinel so that code paths inside the
 * compiled firmware sources are reachable by the coverage instrumentation
 * pass.  No actual hardware I/O is performed.
 *
 * @author Locked, Inc.
 * @date 2026-03-12
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stdint.h>

#include "rx_bus_adc.h"
#include "rx_comm_manager.h"
#include "rx_drv8263.h"
#include "rx_encoder_tpu.h"
#include "rx_err.h"
#include "rx_hcsr04.h"
#include "rx_iwdt.h"
#include "rx_motor.h"
#include "rx_mtu_encoder.h"
#include "rx_obstacle_detect.h"
#include "rx_pid.h"
#include "rx_port_constants.h"
#include "tx_api.h"

/* =============================================================================
 * Global Variables
 * =============================================================================
 */

/**
 * @var g_comm_manager
 * @brief Communication manager handle stub (normally defined in comm_task.c).
 * @details
 * Zero-initialized stub satisfying the external reference from telemetry_task.c
 * and other task sources compiled into firmware_coverage_obj.
 * @note This stub must NOT be included in any build that also links comm_task.c.
 * @since Version 1.0.0
 */
rx_comm_manager_t g_comm_manager;

/**
 * @var g_pin_host_irq
 * @brief HOST_IRQ pin identifier stub (normally defined in hardware_init.c).
 * @details
 * Zero-initialized stub satisfying the external reference from telemetry_task.c.
 * @note This stub must NOT be included in any build that also links hardware_init.c.
 * @since Version 1.0.0
 */
const rx_port_pin_t g_pin_host_irq = (rx_port_pin_t)0;

/* =============================================================================
 * ADC Bus Stub
 * =============================================================================
 */

/**
 * @brief Stub for rx_bus_adc_read_voltage_mv.
 * @param[in] manager Bus manager handle (unused in stub).
 * @param[in] label   ADC channel label (unused in stub).
 * @param[out] voltage_mv_out Output written to zero.
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t
rx_bus_adc_read_voltage_mv(rx_bus_manager_t* manager, const char* bus_name, uint32_t* voltage_mv)
{
  (void)manager;
  (void)bus_name;
  if (voltage_mv != nullptr) {
    *voltage_mv = 0U;
  }
  return k_rx_ok;
}

/* =============================================================================
 * DRV8263 Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_drv8263_init.
 * @param[out] handle Driver handle (unused in stub).
 * @param[in]  config Driver configuration (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_drv8263_init(rx_drv8263_handle_t* handle, const rx_drv8263_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_drv8263_set_drvoff.
 * @param[in] handle Driver handle (unused in stub).
 * @param[in] active DRVOFF state (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_drv8263_set_drvoff(rx_drv8263_handle_t* handle, bool active)
{
  (void)handle;
  (void)active;
  return k_rx_ok;
}

/* =============================================================================
 * MTU Encoder Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_encoder_init.
 * @param[in] config Encoder configuration (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_encoder_init(const rx_encoder_config_t* config)
{
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_encoder_read_count.
 * @param[in]  channel MTU channel (unused in stub).
 * @param[out] state   Encoder state written to zero.
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_encoder_read_count(rx_mtu_channel_t channel, rx_encoder_state_t* state)
{
  (void)channel;
  if (state != nullptr) {
    *state = (rx_encoder_state_t){};
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_encoder_read_velocity.
 * @param[out] velocity_rps Velocity output written to zero.
 * @param[in]  delta_time_s Time step (unused in stub).
 * @param[in]  channel      MTU channel (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_encoder_read_velocity(float* velocity_rps, float delta_time_s, rx_mtu_channel_t channel)
{
  (void)delta_time_s;
  (void)channel;
  if (velocity_rps != nullptr) {
    *velocity_rps = 0.0F;
  }
  return k_rx_ok;
}

/* =============================================================================
 * HC-SR04 Stub
 * =============================================================================
 */

/**
 * @brief Stub for rx_hcsr04_init.
 * @param[out] handle Sensor handle (unused in stub).
 * @param[in]  config Sensor configuration (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_hcsr04_init(rx_hcsr04_t* handle, const rx_hcsr04_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/* =============================================================================
 * IWDT Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_iwdt_check_tasks.
 * @return k_rx_ok always (no tasks monitored in test environment).
 * @since Version 1.0.0
 */
rx_err_t rx_iwdt_check_tasks(void)
{
  return k_rx_ok;
}

/**
 * @brief Stub for rx_iwdt_feed.
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_iwdt_feed(void)
{
  return k_rx_ok;
}

/**
 * @brief Stub for rx_iwdt_task_heartbeat.
 * @param[in] task_name Task name (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_iwdt_task_heartbeat(const char* task_name)
{
  (void)task_name;
  return k_rx_ok;
}

/* =============================================================================
 * Motor Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_motor_get_duty.
 * @param[in]  handle   Motor handle (unused in stub).
 * @param[out] out_duty Duty cycle output written to zero.
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_motor_get_duty(const rx_motor_handle_t* handle, float* out_duty)
{
  (void)handle;
  if (out_duty != nullptr) {
    *out_duty = 0.0F;
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_motor_init.
 * @param[out] handle Motor handle (unused in stub).
 * @param[in]  config Motor configuration (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_motor_init(rx_motor_handle_t* handle, const rx_motor_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_motor_set_duty.
 * @param[in] handle Motor handle (unused in stub).
 * @param[in] duty   Duty cycle (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_motor_set_duty(rx_motor_handle_t* handle, float duty)
{
  (void)handle;
  (void)duty;
  return k_rx_ok;
}

/* =============================================================================
 * Obstacle Detection Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_obstacle_detect_get_stats.
 * @param[in]  handle              Obstacle detection handle (unused in stub).
 * @param[out] out_total_polls     Written to zero.
 * @param[out] out_obstacle_events Written to zero.
 * @param[out] out_false_positives Written to zero.
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_obstacle_detect_get_stats(const rx_obstacle_detect_t* handle,
                                      uint32_t*                   out_total_polls,
                                      uint32_t*                   out_obstacle_events,
                                      uint32_t*                   out_false_positives)
{
  (void)handle;
  if (out_total_polls != nullptr) {
    *out_total_polls = 0U;
  }
  if (out_obstacle_events != nullptr) {
    *out_obstacle_events = 0U;
  }
  if (out_false_positives != nullptr) {
    *out_false_positives = 0U;
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_obstacle_detect_init.
 * @param[out] handle Obstacle detection handle (unused in stub).
 * @param[in]  config Detection configuration (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_obstacle_detect_init(rx_obstacle_detect_t*              handle,
                                 const rx_obstacle_detect_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_obstacle_detect_start.
 * @param[in,out] handle Obstacle detection handle (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_obstacle_detect_start(rx_obstacle_detect_t* handle)
{
  (void)handle;
  return k_rx_ok;
}

/* =============================================================================
 * PID Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_pid_compute.
 * @param[in]  handle   PID handle (unused in stub).
 * @param[in]  setpoint Desired value (unused in stub).
 * @param[in]  measured Measured value (unused in stub).
 * @param[in]  dt       Time step (unused in stub).
 * @param[out] output   Control output written to zero.
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t
rx_pid_compute(rx_pid_handle_t* handle, float setpoint, float measured, float dt, float* output)
{
  (void)handle;
  (void)setpoint;
  (void)measured;
  (void)dt;
  if (output != nullptr) {
    *output = 0.0F;
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_pid_init.
 * @param[out] handle PID handle (unused in stub).
 * @param[in]  config PID configuration (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_pid_init(rx_pid_handle_t* handle, const rx_pid_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_pid_set_gains.
 * @param[in] handle PID handle (unused in stub).
 * @param[in] kp     Proportional gain (unused in stub).
 * @param[in] ki     Integral gain (unused in stub).
 * @param[in] kd     Derivative gain (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_pid_set_gains(rx_pid_handle_t* handle, const float kp, const float ki, const float kd)
{
  (void)handle;
  (void)kp;
  (void)ki;
  (void)kd;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_pid_set_integral_limits.
 * @param[in] handle       PID handle (unused in stub).
 * @param[in] integral_min Minimum integral limit (unused in stub).
 * @param[in] integral_max Maximum integral limit (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_pid_set_integral_limits(rx_pid_handle_t* handle, float integral_min, float integral_max)
{
  (void)handle;
  (void)integral_min;
  (void)integral_max;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_pid_set_output_limits.
 * @param[in] handle     PID handle (unused in stub).
 * @param[in] output_min Minimum output limit (unused in stub).
 * @param[in] output_max Maximum output limit (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_pid_set_output_limits(rx_pid_handle_t* handle, float output_min, float output_max)
{
  (void)handle;
  (void)output_min;
  (void)output_max;
  return k_rx_ok;
}

/* =============================================================================
 * TPU Encoder Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_tpu_encoder_init.
 * @param[in] config TPU encoder configuration (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_encoder_init(const rx_tpu_encoder_config_t* config)
{
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_tpu_encoder_read_count.
 * @param[in]  channel TPU channel (unused in stub).
 * @param[out] state   Encoder state written to zero.
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_encoder_read_count(rx_tpu_channel_t channel, rx_encoder_state_t* state)
{
  (void)channel;
  if (state != nullptr) {
    *state = (rx_encoder_state_t){};
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_tpu_encoder_read_velocity.
 * @param[out] velocity_rps Velocity output written to zero.
 * @param[in]  delta_time_s Time step (unused in stub).
 * @param[in]  channel      TPU channel (unused in stub).
 * @return k_rx_ok always.
 * @since Version 1.0.0
 */
rx_err_t
rx_tpu_encoder_read_velocity(float* velocity_rps, float delta_time_s, rx_tpu_channel_t channel)
{
  (void)delta_time_s;
  (void)channel;
  if (velocity_rps != nullptr) {
    *velocity_rps = 0.0F;
  }
  return k_rx_ok;
}

/* USB0 stubs removed with the USB0 library purge. */

/* =============================================================================
 * ThreadX Stubs
 * =============================================================================
 */

/**
 * @brief Stub for tx_thread_identify.
 * @details
 * Returns nullptr to indicate no active thread in the single-threaded test
 * environment.  Any firmware code that calls tx_thread_identify() and checks
 * the result for nullptr will follow the "not inside a thread" path.
 * @return nullptr always.
 * @since Version 1.0.0
 */
TX_THREAD* tx_thread_identify(void)
{
  return nullptr;
}

/**
 * @brief Stub for tx_thread_suspend.
 * @param[in] thread_ptr ThreadX thread control block (unused in stub).
 * @return TX_SUCCESS always.
 * @since Version 1.0.0
 */
UINT tx_thread_suspend(TX_THREAD* thread_ptr)
{
  (void)thread_ptr;
  return TX_SUCCESS;
}

/**
 * @brief Stub for tx_time_get.
 * @details Returns 0 to indicate t=0 in the single-threaded test environment.
 * @return 0 always.
 * @since Version 1.0.0
 */
ULONG tx_time_get(void)
{
  return 0U;
}

/**
 * @brief Stub for mock_tx_set_time.
 * @param[in] ticks Desired tick count (ignored in stub).
 * @since Version 1.0.0
 */
void mock_tx_set_time(ULONG ticks)
{
  (void)ticks;
}

/* =============================================================================
 * Mock Reset Stubs
 * =============================================================================
 */

/**
 * @brief Stub for mock_tasks_reset.
 * @details
 * Called by mock_tx_reset() to reset task mock state. In test_coverage_compile_all
 * we do not include mock_tasks.c (its task-create symbols conflict with the real
 * task files compiled in firmware_coverage_obj), so we provide this no-op stub.
 * @since Version 1.0.0
 */
void mock_tasks_reset(void)
{
  /* No-op: task mock state not used in coverage baseline test. */
}
