/**
 * @file motor_control_task.c
 * @brief FreeRTOS task implementation for motor control
 * @details
 * Implements the 250Hz motor control task that runs 4 independent PID controllers
 * for skid-steer robot control. The task reads multiplexed encoders, computes PID
 * outputs, applies motor commands, and communicates status through shared state.
 *
 * Control Loop Timing (4ms period):
 * - Read 4 encoders: ~80μs
 * - Read setpoints: ~20μs
 * - Compute 4 PIDs: ~80μs
 * - Apply motors: ~80μs
 * - Update status: ~100μs
 * - Check estop: ~20μs
 * Total: ~380μs (9.5% of 4ms period)
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "tasks/motor_control_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>

#include "esp_log.h"

/* --- Constants --- */

/**
 * @brief Logging tag for ESP-IDF logging macros
 */
static const char* s_tag = "MOTOR_CONTROL_TASK";

/**
 * @brief Pi constant for velocity conversion
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- Type Definitions --- */

/**
 * @brief Task parameters structure
 *
 * Holds pointers to all resources needed by the motor control task.
 * Passed to the task via xTaskCreate's pvParameters.
 */
typedef struct {
  motor_shared_state_t*      shared_state; /**< Pointer to shared state */
  star_motor_handle_t*       motors;       /**< Array of 4 motor handles */
  star_encoder_mux_handle_t* encoder_mux;  /**< Pointer to encoder mux */
  star_pid_handle_t*         pids;         /**< Array of 4 PID handles */
} motor_task_params_t;

/* --- Private Function Prototypes --- */

static void internal_motor_control_loop(motor_task_params_t* params);
static void internal_motor_control_task(void* pvParameters);
static void internal_convert_rpm_to_mps(const float velocities_rpm[k_motor_count],
                                        float       velocities_mps[k_motor_count]);
static void internal_convert_mps_to_rpm(const float velocities_mps[k_motor_count],
                                        float       velocities_rpm[k_motor_count]);

/* --- Public Functions --- */

void motor_control_task_create(motor_shared_state_t*      shared_state,
                               star_motor_handle_t        motors[k_motor_count],
                               star_encoder_mux_handle_t* encoder_mux,
                               star_pid_handle_t          pids[k_motor_count])
{
  /* Allocate task parameters (static to persist after function returns) */
  static motor_task_params_t task_params;
  task_params.shared_state = shared_state;
  task_params.motors       = motors;
  task_params.encoder_mux  = encoder_mux;
  task_params.pids         = pids;

  /* Create motor control task */
  BaseType_t task_ret = xTaskCreate(internal_motor_control_task,
                                    "motor_ctrl",
                                    k_motor_control_task_stack_size,
                                    &task_params,
                                    k_motor_control_task_priority,
                                    NULL);

  if (task_ret != pdPASS) {
    ESP_LOGE(s_tag, "Failed to create motor control task");
    return;
  }

  ESP_LOGI(s_tag,
           "Motor control task created at %dHz (priority %d)",
           k_motor_control_loop_hz,
           k_motor_control_task_priority);
}

/* --- Private Functions --- */

/**
 * @brief Main motor control loop (250Hz)
 *
 * Implements the 250Hz control loop that:
 * 1. Reads all 4 encoders via multiplexer
 * 2. Converts RPM to m/s
 * 3. Reads velocity setpoints from shared state
 * 4. Computes PID outputs for all motors
 * 5. Applies outputs to motors
 * 6. Updates shared state with status
 * 7. Checks for emergency stop
 * 8. Waits for next period (vTaskDelayUntil for precise timing)
 *
 * @param[in] params Pointer to task parameters
 */
static void internal_motor_control_loop(motor_task_params_t* params)
{
  const TickType_t period_ticks   = pdMS_TO_TICKS(k_motor_control_period_ms);
  TickType_t       last_wake_time = xTaskGetTickCount();
  const float      dt_s           = k_motor_control_period_ms / 1000.0f;

  int32_t positions[k_motor_count];
  float   velocities_rpm[k_motor_count];
  float   velocities_mps[k_motor_count];
  float   setpoints_mps[k_motor_count];
  float   outputs[k_motor_count];

  ESP_LOGI(s_tag, "Motor control loop started at %dHz", k_motor_control_loop_hz);

  while (true) {
    /* 1. Read all 4 encoders (~80μs) */
    esp_err_t ret = star_encoder_mux_read_all(params->encoder_mux,
                                              positions,
                                              velocities_rpm,
                                              k_motor_control_period_ms,
                                              k_encoder_ppr);

    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to read encoders: %s", esp_err_to_name(ret));
      /* Continue with last known values */
    }

    /* 2. Convert RPM to m/s */
    internal_convert_rpm_to_mps(velocities_rpm, velocities_mps);

    /* 3. Read velocity setpoints from shared state (~20μs per motor) */
    for (int i = 0; i < k_motor_count; i++) {
      ret = motor_shared_state_get_velocity(params->shared_state, i, &setpoints_mps[i]);
      if (ret != ESP_OK) {
        /* On timeout, use last known setpoint (already in setpoints_mps) */
        ESP_LOGD(s_tag, "Timeout reading setpoint for motor %d, using cached value", i);
      }
    }

    /* 4. Compute PID for all 4 motors (~20μs per motor) */
    for (int i = 0; i < k_motor_count; i++) {
      ret =
        star_pid_compute(&params->pids[i], setpoints_mps[i], velocities_mps[i], dt_s, &outputs[i]);

      if (ret != ESP_OK) {
        ESP_LOGW(s_tag, "PID compute failed for motor %d: %s", i, esp_err_to_name(ret));
        outputs[i] = 0.0f; /* Safe fallback */
      }
    }

    /* 5. Apply outputs to motors (~20μs per motor) */
    for (int i = 0; i < k_motor_count; i++) {
      ret = star_motor_set_duty(&params->motors[i], outputs[i]);

      if (ret != ESP_OK) {
        ESP_LOGW(s_tag, "Failed to set motor %d duty: %s", i, esp_err_to_name(ret));
      }
    }

    /* 6. Update shared state with status (~25μs per motor) */
    for (int i = 0; i < k_motor_count; i++) {
      star_v1_MotorState motor_state;

      /* Determine motor state */
      if (fabsf(setpoints_mps[i]) < 0.01f && fabsf(outputs[i]) < 1.0f) {
        motor_state = star_v1_MotorState_MOTOR_STATE_IDLE;
      } else {
        motor_state = star_v1_MotorState_MOTOR_STATE_RUNNING;
      }

      /* Update status (ignore timeout - not critical) */
      motor_shared_state_set_status(params->shared_state,
                                    i,
                                    velocities_mps[i],
                                    outputs[i],
                                    0.0f, /* TODO: Add current sensing */
                                    motor_state);
    }

    /* 7. Check emergency stop (~20μs) */
    bool estop_active;
    ret = motor_shared_state_get_estop(params->shared_state, &estop_active);

    if (ret == ESP_ERR_TIMEOUT) {
      /* Fail-safe: assume emergency stop on mutex timeout */
      estop_active = true;
    }

    if (estop_active) {
      /* Emergency stop: brake all motors */
      for (int i = 0; i < k_motor_count; i++) {
        star_motor_stop(&params->motors[i], true); /* Brake */

        /* Update state to ESTOP */
        motor_shared_state_set_status(params->shared_state,
                                      i,
                                      0.0f,
                                      0.0f,
                                      0.0f,
                                      star_v1_MotorState_MOTOR_STATE_ESTOP);
      }
    }

    /* 8. Wait for next period (maintains precise 250Hz) */
    vTaskDelayUntil(&last_wake_time, period_ticks);
  }
}

/**
 * @brief Motor control task entry point
 *
 * FreeRTOS task entry point. Extracts parameters and calls the main control loop.
 *
 * @param[in] pvParameters Pointer to motor_task_params_t
 */
static void internal_motor_control_task(void* pvParameters)
{
  motor_task_params_t* params = (motor_task_params_t*)pvParameters;

  ESP_LOGI(s_tag, "Motor control task started");

  /* Run control loop */
  internal_motor_control_loop(params);

  /* Should never reach here */
  ESP_LOGE(s_tag, "Motor control task exited unexpectedly");
  vTaskDelete(NULL);
}

/**
 * @brief Convert velocities from RPM to m/s
 *
 * Converts encoder velocities from revolutions per minute to meters per second
 * based on wheel diameter.
 *
 * Formula: velocity_mps = velocity_rpm * (π * diameter_m / 60)
 *
 * @param[in]  velocities_rpm Array of 4 velocities in RPM
 * @param[out] velocities_mps Array of 4 velocities in m/s
 */
static void internal_convert_rpm_to_mps(const float velocities_rpm[k_motor_count],
                                        float       velocities_mps[k_motor_count])
{
  const float diameter_m        = k_wheel_diameter_m / 1000.0f; /* mm to m */
  const float conversion_factor = (M_PI * diameter_m) / 60.0f;

  for (int i = 0; i < k_motor_count; i++) {
    velocities_mps[i] = velocities_rpm[i] * conversion_factor;
  }
}

/**
 * @brief Convert velocities from m/s to RPM
 *
 * Converts velocities from meters per second to revolutions per minute
 * based on wheel diameter. Currently unused but provided for completeness.
 *
 * Formula: velocity_rpm = velocity_mps / (π * diameter_m / 60)
 *
 * @param[in]  velocities_mps Array of 4 velocities in m/s
 * @param[out] velocities_rpm Array of 4 velocities in RPM
 */
static void internal_convert_mps_to_rpm(const float velocities_mps[k_motor_count],
                                        float       velocities_rpm[k_motor_count])
{
  const float diameter_m        = k_wheel_diameter_m / 1000.0f; /* mm to m */
  const float conversion_factor = 60.0f / (M_PI * diameter_m);

  for (int i = 0; i < k_motor_count; i++) {
    velocities_rpm[i] = velocities_mps[i] * conversion_factor;
  }
}
