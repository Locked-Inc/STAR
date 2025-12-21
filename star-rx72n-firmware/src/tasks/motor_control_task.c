/* src/tasks/motor_control_task.c */

/**
 * @file motor_control_task.c
 * @brief ThreadX task implementation for motor control
 * @details
 * Implements the 250Hz motor control task that runs 4 independent PID controllers
 * for skid-steer robot control. The task reads encoders from dedicated MTU channels,
 * computes PID outputs, applies motor commands, and communicates status through shared state.
 *
 * Port of ESP32 motor_control_task using ThreadX RTOS and RX72N peripherals.
 *
 * Control Loop Timing (4ms period, 250Hz):
 * - Read 4 encoders: ~40μs (no mux, direct MTU reads)
 * - Read setpoints: ~20μs
 * - Compute 4 PIDs: ~80μs
 * - Apply motors: ~80μs
 * - Update status: ~100μs
 * - Check estop: ~20μs
 * Total: ~340μs (8.5% of 4ms period)
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "tasks/motor_control_task.h"

#include "hardware_config.h"
#include "rx_check.h"
#include "rx_cmt.h"
#include "rx_log.h"

#include <math.h>
#include <string.h>

static const char* s_tag = "MOTOR_CTRL";

/**
 * @brief Pi constant for velocity conversion
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief Task parameters structure
 */
typedef struct {
  motor_shared_state_t* shared_state;     /**< Pointer to shared state */
  rx_motor_handle_t*    motors;           /**< Array of 4 motor handles */
  rx_mtu_channel_t*     encoder_channels; /**< Array of 4 encoder MTU channels */
  rx_pid_handle_t*      pids;             /**< Array of 4 PID handles */
} motor_task_params_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static TX_THREAD            s_motor_control_thread;
static uint8_t              s_motor_control_stack[k_motor_control_task_stack_size];
static motor_task_params_t  s_task_params;
static TX_SEMAPHORE         s_control_loop_semaphore;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert velocities from RPM to m/s
 *
 * @param[in]  velocities_rpm Array of 4 velocities in RPM
 * @param[out] velocities_mps Array of 4 velocities in m/s
 */
static void internal_convert_rpm_to_mps(const float velocities_rpm[k_motor_count],
                                        float       velocities_mps[k_motor_count]);

/**
 * @brief Main motor control loop (250Hz)
 *
 * @param[in] params Pointer to task parameters
 */
static void internal_motor_control_loop(motor_task_params_t* params);

/**
 * @brief Motor control task entry point
 *
 * @param[in] input ThreadX task input parameter (motor_task_params_t*)
 */
static void internal_motor_control_task(ULONG input);

/**
 * @brief CMT1 callback for 250Hz control loop timing
 *
 * @param[in] user_data User data (unused)
 */
static void internal_cmt1_callback(void* user_data);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t motor_control_task_create(motor_shared_state_t* shared_state,
                                    rx_motor_handle_t     motors[k_motor_count],
                                    rx_mtu_channel_t      encoder_channels[k_motor_count],
                                    rx_pid_handle_t       pids[k_motor_count])
{
  RX_CHECK_NULL_PTR(shared_state, s_tag, "shared_state pointer is NULL");
  RX_CHECK_NULL_PTR(motors, s_tag, "motors pointer is NULL");
  RX_CHECK_NULL_PTR(encoder_channels, s_tag, "encoder_channels pointer is NULL");
  RX_CHECK_NULL_PTR(pids, s_tag, "pids pointer is NULL");

  /* Store task parameters */
  s_task_params.shared_state     = shared_state;
  s_task_params.motors           = motors;
  s_task_params.encoder_channels = encoder_channels;
  s_task_params.pids             = pids;

  /* Create semaphore for 250Hz timing */
  UINT status = tx_semaphore_create(&s_control_loop_semaphore, "motor_ctrl_sem", 0);
  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_STATE;
  }

  /* Initialize CMT1 for 250Hz timing */
  rx_cmt_config_t cmt_config = {
    .frequency_hz = k_motor_control_loop_hz,
    .callback     = internal_cmt1_callback,
    .user_data    = NULL,
    .priority     = 5, /* High priority for precise timing */
  };

  rx_err_t err = rx_cmt_init(k_cmt_channel_1, &cmt_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    tx_semaphore_delete(&s_control_loop_semaphore);
    return err;
  }

  /* Create motor control thread */
  status = tx_thread_create(&s_motor_control_thread,
                           "motor_ctrl",
                           internal_motor_control_task,
                           0, /* Input parameter (unused, using static params) */
                           s_motor_control_stack,
                           k_motor_control_task_stack_size,
                           k_motor_control_task_priority,
                           k_motor_control_task_priority,
                           TX_NO_TIME_SLICE,
                           TX_AUTO_START);

  if (status != TX_SUCCESS) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    rx_cmt_deinit(k_cmt_channel_1);
    tx_semaphore_delete(&s_control_loop_semaphore);
    return RX_ERR_INVALID_STATE;
  }

  RX_LOG_INFO(s_tag, "Motor control task created");

  return RX_OK;
}

/* =============================================================================
 * Internal Helper Functions Implementation
 * =============================================================================
 */

static void internal_cmt1_callback(void* user_data)
{
  (void)user_data;

  /* Post semaphore to wake up control loop */
  tx_semaphore_put(&s_control_loop_semaphore);
}

static void internal_motor_control_loop(motor_task_params_t* params)
{
  const float dt_s = k_motor_control_period_ms / 1000.0f;

  rx_encoder_state_t encoder_states[k_motor_count];
  float              velocities_rpm[k_motor_count];
  float              velocities_mps[k_motor_count];
  float              setpoints_mps[k_motor_count];
  float              outputs[k_motor_count];

  /* Initialize encoder states */
  memset(encoder_states, 0, sizeof(encoder_states));
  memset(setpoints_mps, 0, sizeof(setpoints_mps));

  RX_LOG_INFO(s_tag, "Info");

  while (true) {
    /* Wait for 250Hz timer (CMT1 callback posts semaphore) */
    UINT status = tx_semaphore_get(&s_control_loop_semaphore, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
      RX_LOG_ERROR(s_tag, "Error occurred");
      continue;
    }

    /* 1. Read all 4 encoders directly from MTU channels (~10μs each) */
    for (int i = 0; i < k_motor_count; i++) {
      rx_err_t err = rx_encoder_read_count(params->encoder_channels[i], &encoder_states[i]);
      if (err != RX_OK) {
        RX_LOG_WARN(s_tag, "Warning");
        /* Continue with last known values */
      }

      /* Calculate velocity in RPM */
      err = rx_encoder_read_velocity(params->encoder_channels[i],
                                      dt_s,
                                      &velocities_rpm[i]);
      if (err != RX_OK) {
        RX_LOG_WARN(s_tag, "Warning");
        velocities_rpm[i] = 0.0f;
      }
    }

    /* 2. Convert RPM to m/s */
    internal_convert_rpm_to_mps(velocities_rpm, velocities_mps);

    /* 3. Read velocity setpoints from shared state (~5μs per motor) */
    for (int i = 0; i < k_motor_count; i++) {
      rx_err_t err = motor_shared_state_get_velocity(params->shared_state, i, &setpoints_mps[i]);
      if (err != RX_OK) {
        /* On timeout, use last known setpoint (already in setpoints_mps) */
        RX_LOG_DEBUG(s_tag, "Debug");
      }
    }

    /* 4. Compute PID for all 4 motors (~20μs per motor) */
    for (int i = 0; i < k_motor_count; i++) {
      rx_err_t err =
        rx_pid_compute(&params->pids[i], setpoints_mps[i], velocities_mps[i], dt_s, &outputs[i]);

      if (err != RX_OK) {
        RX_LOG_WARN(s_tag, "Warning");
        outputs[i] = 0.0f; /* Safe fallback */
      }
    }

    /* 5. Apply outputs to motors (~20μs per motor) */
    for (int i = 0; i < k_motor_count; i++) {
      rx_err_t err = rx_motor_set_duty(&params->motors[i], outputs[i]);
      if (err != RX_OK) {
        RX_LOG_WARN(s_tag, "Warning");
      }
    }

    /* 6. Update shared state with status (~25μs per motor) */
    for (int i = 0; i < k_motor_count; i++) {
      rx_motor_state_t motor_state;

      /* Determine motor state */
      if (fabsf(setpoints_mps[i]) < 0.01f && fabsf(outputs[i]) < 1.0f) {
        motor_state = k_motor_state_idle;
      } else {
        motor_state = k_motor_state_running;
      }

      /* Update status (ignore timeout - not critical) */
      motor_shared_state_set_status(params->shared_state,
                                    i,
                                    velocities_mps[i],
                                    outputs[i],
                                    0.0f, /* TODO: Add current sensing via rx_drv8243 */
                                    motor_state);
    }

    /* 7. Check emergency stop (~5μs) */
    bool     estop_active;
    rx_err_t err = motor_shared_state_get_estop(params->shared_state, &estop_active);

    if (err == RX_ERR_TIMEOUT) {
      /* Fail-safe: assume emergency stop on mutex timeout */
      estop_active = true;
    }

    if (estop_active) {
      /* Emergency stop: brake all motors */
      for (int i = 0; i < k_motor_count; i++) {
        rx_motor_stop(&params->motors[i], true); /* Brake */

        /* Update state to FAULT (using as ESTOP equivalent) */
        motor_shared_state_set_status(params->shared_state,
                                      i,
                                      0.0f,
                                      0.0f,
                                      0.0f,
                                      k_motor_state_fault);
      }
    }
  }
}

static void internal_motor_control_task(ULONG input)
{
  (void)input;

  RX_LOG_INFO(s_tag, "Motor control task started");

  /* Run control loop */
  internal_motor_control_loop(&s_task_params);

  /* Should never reach here */
  RX_LOG_ERROR(s_tag, "Motor control task exited unexpectedly");
  tx_thread_delete(&s_motor_control_thread);
}

static void internal_convert_rpm_to_mps(const float velocities_rpm[k_motor_count],
                                        float       velocities_mps[k_motor_count])
{
  const float diameter_m        = k_wheel_diameter_mm / 1000.0f; /* mm to m */
  const float conversion_factor = (M_PI * diameter_m) / 60.0f;

  for (int i = 0; i < k_motor_count; i++) {
    velocities_mps[i] = velocities_rpm[i] * conversion_factor;
  }
}
