/* src/tasks/motor_controller.c */

/**
 * @file motor_controller.c
 * @brief Motor Controller thread implementation
 * @details
 * Deterministic 250 Hz control loop for 4-motor velocity control.
 * Highest priority thread to prevent jitter from sensor delays.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/motor_controller.h"

#include <string.h>

#include "hardware_pinout.h"
#include "motor_config.h"
#include "rx_bus_manager.h"
#include "system_config.h"
#include "rx_drv8243.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_mtu_encoder.h"
#include "rx_pid.h"
#include "shared_state.h"

/* =============================================================================
 * Private Types
 * =============================================================================
 */

/**
 * @brief Motor subsystem state
 * @details Holds handles for all motor control components
 */
typedef struct {
  rx_bus_manager_t    bus_manager;                    /**< Bus manager for GPIO/ADC access */
  rx_drv8243_handle_t motor_drivers[k_motor_count];   /**< DRV8243S motor driver handles */
  rx_pid_handle_t     pid_controllers[k_motor_count]; /**< PID controller handles */
  rx_encoder_state_t  encoder_states[k_motor_count];  /**< Encoder state tracking */
  bool                initialized;                    /**< True if subsystem initialized */
} motor_subsystem_t;

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static char* s_tag       = "motor_ctrl";
static char* s_task_name = "Motor_Controller";

/* Thread control block and stack (statically allocated) */
static TX_THREAD s_motor_controller_thread;
static uint8_t   s_motor_controller_stack[k_stack_motor_controller];

/* Motor subsystem state */
static motor_subsystem_t s_motor_subsystem;

/* Module constants */
typedef enum {
  k_watchdog_timeout_ms = 12,  /**< 3x period = 3 * 4ms = 12ms */
  k_port_shift          = 8,   /**< Bit shift to extract port from gpio_pin_t */
  k_port_mask           = 0xFF, /**< Bit mask to extract port or pin */
} motor_controller_constants_t;

/* PWM duty cycle conversion factor */
static const float s_duty_cycle_scale = 100.0f; /**< Converts PID output (-100 to +100) to duty (-1.0 to +1.0) */

/* =============================================================================
 * Private Function Prototypes
 * =============================================================================
 */

/**
 * @brief Motor Controller thread entry point
 * @param input Thread input parameter (unused)
 */
static void motor_controller_entry(ULONG input);

/**
 * @brief Initialize motor subsystem (motors, encoders, PIDs)
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t motor_subsystem_init(void);
static rx_err_t init_bus_manager(void);
static rx_err_t init_pid_controllers(void);
static rx_err_t init_motor_drivers(void);
static rx_err_t init_encoder_states(void);

/**
 * @brief Execute one control loop iteration
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t control_loop_iteration(void);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

UINT motor_controller_create(void)
{
  UINT status = TX_SUCCESS;

  /* Zero-initialize motor subsystem */
  memset(&s_motor_subsystem, 0, sizeof(motor_subsystem_t));

  /* Create Motor_Controller thread */
  status = tx_thread_create(&s_motor_controller_thread,
                            s_tag,
                            motor_controller_entry,
                            0,
                            s_motor_controller_stack,
                            k_stack_motor_controller,
                            k_priority_motor_controller,
                            k_priority_motor_controller,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Thread creation failed");
    return status;
  }

  rx_log_info(s_tag, "Motor_Controller thread created (priority 2, 250 Hz)");
  return TX_SUCCESS;
}

/* =============================================================================
 * Private Function Implementations
 * =============================================================================
 */

static void motor_controller_entry(ULONG input)
{
  rx_err_t ret = k_rx_ok;

  (void)input; /* Suppress unused parameter warning */

  rx_log_info(s_tag, "Motor_Controller thread started");

  /* Register with watchdog for task-level monitoring
     * Timeout = 3x period = 3 * 4ms = 12ms */
  ret = rx_iwdt_register_task(s_task_name, k_watchdog_timeout_ms);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Failed to register with watchdog");
  }

  /* Initialize motor subsystem */
  ret = motor_subsystem_init();
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Motor subsystem init failed");
    /* Cannot continue - halt this thread */
    while (1) {
      tx_thread_sleep(k_threadx_ticks_1s); /* Sleep until reset */
    }
  }

  rx_log_info(s_tag, "Entering 250 Hz control loop");

  /* Main control loop (250 Hz = 4ms period) */
  while (1) {
    /* Execute control loop iteration */
    ret = control_loop_iteration();
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "Control loop error");
    }

    /* Feed watchdog (critical - prevents reset) */
    rx_iwdt_feed();

    /* Record task heartbeat for deadlock detection */
    rx_iwdt_task_heartbeat(s_task_name);

    /* Sleep for 4ms (250 Hz control rate)
         * Note: k_control_loop_ticks = 0 because 4ms < 10ms ThreadX tick
         *
         * Phase 1: Using tx_thread_sleep(k_threadx_ticks_10ms) = 10ms for stub implementation
         * Phase 2: Will use dedicated timer interrupt for precise 4ms timing
         *          when actual motor control hardware is integrated */
    tx_thread_sleep(
      k_threadx_ticks_10ms); /* 10ms tick - Phase 2 will implement precise 4ms timer */
  }
}

/**
 * @brief Initialize bus manager for motor subsystem
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t init_bus_manager(void)
{
  rx_err_t ret = rx_bus_manager_init(&s_motor_subsystem.bus_manager, s_tag, NULL, NULL);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Bus manager init failed");
    return ret;
  }

  rx_log_info(s_tag, "Bus manager initialized");
  return k_rx_ok;
}

/**
 * @brief Initialize PID controllers for all 4 motors
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t init_pid_controllers(void)
{
  const float kp      = (float)k_pid_kp_x1000 / (float)k_pid_gain_scale_factor;
  const float ki      = (float)k_pid_ki_x1000 / (float)k_pid_gain_scale_factor;
  const float kd      = (float)k_pid_kd_x1000 / (float)k_pid_gain_scale_factor;
  const float out_min = -(float)k_pid_output_max_percent;
  const float out_max = (float)k_pid_output_max_percent;
  const float int_min = (float)k_pid_integral_min_x100 / (float)k_pid_integral_scale_factor;
  const float int_max = (float)k_pid_integral_max_x100 / (float)k_pid_integral_scale_factor;

  for (uint8_t i = 0; i < k_motor_count; i++) {
    rx_pid_config_t pid_config = {
      .kp           = kp,
      .ki           = ki,
      .kd           = kd,
      .output_min   = out_min,
      .output_max   = out_max,
      .integral_min = int_min,
      .integral_max = int_max,
    };

    rx_err_t ret = rx_pid_init(&s_motor_subsystem.pid_controllers[i], &pid_config);
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "PID init failed");
      return ret;
    }
  }

  rx_log_info(s_tag, "PID controllers initialized (Kp=0.286, Ki=8.01)");
  return k_rx_ok;
}

/**
 * @brief Initialize DRV8243S motor drivers for all 4 motors
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t init_motor_drivers(void)
{
  const uint8_t adc_pins_ipropi[k_motor_count] = {
    k_adc_motor0_current,
    k_adc_motor1_current,
    k_adc_motor2_current,
    k_adc_motor3_current,
  };

  const gpio_pin_t fault_pins[k_motor_count] = {
    k_pin_motor0_nfault,
    k_pin_motor1_nfault,
    k_pin_motor2_nfault,
    k_pin_motor3_nfault,
  };

  for (uint8_t i = 0; i < k_motor_count; i++) {
    rx_drv8243_config_t motor_config = {
      .bus_manager      = &s_motor_subsystem.bus_manager,
      .gpio_bus_name    = "motor_gpio",
      .adc_bus_name     = "motor_adc",
      .gptw_channel     = (rx_gptw_channel_t)i,
      .output_ph        = k_gptw_output_a,
      .output_en        = k_gptw_output_b,
      .pin_ipropi       = adc_pins_ipropi[i],
      .port_nfault      = (uint8_t)((fault_pins[i] >> k_port_shift) & k_port_mask),
      .pin_nfault       = (uint8_t)(fault_pins[i] & k_port_mask),
      .pwm_freq_hz      = k_pwm_frequency_hz,
      .current_limit_ma = k_drv8243_max_current_ma,
      .ki_propi         = (uint16_t)(k_drv8243_ipropi_scaling_x100 / 100),
    };

    rx_err_t ret = rx_drv8243_init(&s_motor_subsystem.motor_drivers[i], &motor_config);
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "Motor driver init failed");
      return ret;
    }
  }

  rx_log_info(s_tag, "Motor drivers initialized (4x DRV8243S, 20 kHz PWM)");
  return k_rx_ok;
}

/**
 * @brief Initialize encoder state tracking structures
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t init_encoder_states(void)
{
  for (uint8_t i = 0; i < k_motor_count; i++) {
    memset(&s_motor_subsystem.encoder_states[i], 0, sizeof(rx_encoder_state_t));
  }

  rx_log_info(s_tag, "Encoder state tracking initialized");
  return k_rx_ok;
}

static rx_err_t motor_subsystem_init(void)
{
  /* -------------------------------------------------------------------------
     * Issue 6: Motor Subsystem Initialization (Refactored)
     * -------------------------------------------------------------------------
     * Refactored into 4 single-responsibility helper functions for NASA Rule 4
     * compliance (functions ≤60 lines)
     */
  rx_err_t ret;

  /* 1. Initialize bus manager (GPIO/ADC/SPI abstraction) */
  ret = init_bus_manager();
  if (ret != k_rx_ok) {
    return ret;
  }

  /* 2. Initialize PID controllers (MATLAB-tuned gains) */
  ret = init_pid_controllers();
  if (ret != k_rx_ok) {
    return ret;
  }

  /* 3. Initialize DRV8243S motor drivers (4 motors, 20 kHz PWM) */
  ret = init_motor_drivers();
  if (ret != k_rx_ok) {
    return ret;
  }

  /* 4. Initialize encoder state tracking structures */
  ret = init_encoder_states();
  if (ret != k_rx_ok) {
    return ret;
  }

  s_motor_subsystem.initialized = true;
  rx_log_info(s_tag, "Motor subsystem initialization complete");

  return k_rx_ok;
}

/**
 * @brief Check emergency stop status and halt motors if active
 *
 * @return k_rx_ok if E-STOP not active, k_rx_err_estop if active
 */
static rx_err_t check_emergency_stop(void)
{
  shared_state_t* state = shared_state_get();

  UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire safety mutex");
    return k_rx_err_threadx;
  }

  const bool emergency_stop = state->safety.emergency_stop;
  tx_mutex_put(&state->safety_mutex);

  if (emergency_stop) {
    /* E-STOP active - halt all motors with brake */
    for (uint8_t i = 0; i < k_motor_count; i++) {
      rx_drv8243_stop(&s_motor_subsystem.motor_drivers[i], true);
    }
    return k_rx_err_estop; /* Signal E-STOP is active */
  }

  return k_rx_ok;
}

/**
 * @brief Read encoder feedback and update shared state
 *
 * Reads encoder counts for all motors, computes velocities, and updates
 * shared state encoder feedback structure.
 *
 * @param[in] dt Time delta in seconds for velocity calculation
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t read_encoder_feedback(float dt)
{
  shared_state_t* state = shared_state_get();

  for (uint8_t i = 0; i < k_motor_count; i++) {
    /* Read encoder state (updates total_count, revolutions, position) */
    rx_encoder_state_t* encoder_state = &s_motor_subsystem.encoder_states[i];
    const int32_t       prev_count    = encoder_state->total_count;

    rx_err_t ret = rx_encoder_read_count((rx_mtu_channel_t)(i + 1), encoder_state);
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "Encoder read failed");
      continue;
    }

    /* Calculate delta count for velocity */
    const int32_t delta_count = encoder_state->total_count - prev_count;

    /* Compute velocity (counts per second) */
    const float velocity_cps = (float)delta_count / dt;

    /* Convert to revolutions per second */
    const float velocity_rps = velocity_cps / (float)k_encoder_cpr;

    /* Convert to linear velocity (m/s) assuming wheel radius */
    const float wheel_circumference_m = 2.0f * 3.14159f * ((float)k_wheel_radius_mm / 1000.0f);
    const float velocity_mps          = velocity_rps * wheel_circumference_m;

    /* Update shared encoder feedback */
    UINT status = tx_mutex_get(&state->encoder_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
      rx_log_error(s_tag, "Failed to acquire encoder mutex");
      continue;
    }

    state->encoders.motors[i].ticks        = encoder_state->total_count;
    state->encoders.motors[i].velocity_mps = velocity_mps;
    state->encoders.motors[i].timestamp_ms = tx_time_get(); /* ThreadX ticks */

    tx_mutex_put(&state->encoder_mutex);
  }

  return k_rx_ok;
}

/**
 * @brief Read motor velocity setpoints from shared state
 *
 * @param[out] target_velocities Array to store target velocities (m/s)
 * @param[out] setpoint_valid Pointer to store setpoint validity flag
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t read_motor_setpoint(float* target_velocities, bool* setpoint_valid)
{
  shared_state_t* state = shared_state_get();

  UINT status = tx_mutex_get(&state->setpoint_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire setpoint mutex");
    return k_rx_err_threadx;
  }

  *setpoint_valid = state->setpoint.valid;
  for (uint8_t i = 0; i < k_motor_count; i++) {
    target_velocities[i] = state->setpoint.velocity_mps[i];
  }

  tx_mutex_put(&state->setpoint_mutex);

  return k_rx_ok;
}

/**
 * @brief Compute PID control and apply motor commands
 *
 * For each motor: reads current velocity, runs PID controller, and applies
 * duty cycle command to motor driver.
 *
 * @param[in] target_velocities Array of target velocities (m/s)
 * @param[in] dt Time delta in seconds for PID computation
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t compute_and_apply_pid(const float* target_velocities, float dt)
{
  shared_state_t* state = shared_state_get();

  for (uint8_t i = 0; i < k_motor_count; i++) {
    /* Get current velocity from shared state */
    UINT status = tx_mutex_get(&state->encoder_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
      rx_log_error(s_tag, "Failed to acquire encoder mutex");
      continue;
    }

    const float measured_velocity = state->encoders.motors[i].velocity_mps;
    tx_mutex_put(&state->encoder_mutex);

    /* Compute PID output (duty cycle percentage) */
    float    output = 0.0f;
    rx_err_t ret    = rx_pid_compute(&s_motor_subsystem.pid_controllers[i],
                                  target_velocities[i],
                                  measured_velocity,
                                  dt,
                                  &output);

    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "PID compute failed");
      continue;
    }

    /* Apply duty cycle to motor driver (-100% to +100%) */
    ret = rx_drv8243_set_speed(&s_motor_subsystem.motor_drivers[i], output / s_duty_cycle_scale);
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "Motor set speed failed");
    }
  }

  return k_rx_ok;
}

/**
 * @brief Check for motor faults and update safety state
 *
 * Reads motor currents and nFAULT pins, triggers E-STOP on overcurrent
 * or fault detection.
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t check_motor_faults(void)
{
  shared_state_t* state = shared_state_get();

  for (uint8_t i = 0; i < k_motor_count; i++) {
    float    current_ma = 0.0f;
    rx_err_t ret        = rx_drv8243_read_current(&s_motor_subsystem.motor_drivers[i], &current_ma);
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "Current read failed");
      continue;
    }

    /* Check for overcurrent */
    if (current_ma > (float)k_drv8243_overcurrent_ma) {
      rx_log_error(s_tag, "Motor overcurrent detected");

      /* Set fault flag in safety state */
      UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
      if (status == TX_SUCCESS) {
        state->safety.fault_flags |= (1 << i);
        state->safety.emergency_stop = true; /* Trigger E-STOP on overcurrent */
        tx_mutex_put(&state->safety_mutex);
      }
    }

    /* Check for nFAULT pin */
    bool fault_detected = false;
    ret = rx_drv8243_get_fault_status(&s_motor_subsystem.motor_drivers[i], &fault_detected);
    if (ret == k_rx_ok && fault_detected) {
      rx_log_error(s_tag, "Motor driver fault (nFAULT)");

      /* Set fault flag and trigger E-STOP */
      UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
      if (status == TX_SUCCESS) {
        state->safety.fault_flags |= (1 << i);
        state->safety.emergency_stop = true;
        tx_mutex_put(&state->safety_mutex);
      }
    }
  }

  return k_rx_ok;
}

static rx_err_t control_loop_iteration(void)
{
  /* -------------------------------------------------------------------------
     * Issue 7: Deterministic 250 Hz Motor Control Loop
     * -------------------------------------------------------------------------
     * Refactored into 5 single-responsibility helper functions for NASA Rule 4
     * compliance (functions ≤60 lines)
     */
  rx_err_t    ret;
  float       target_velocities[k_motor_count];
  bool        setpoint_valid = false;
  const float dt             = (float)k_control_loop_period_ms / 1000.0f; /* 0.004s (4ms) */

  /* 1. Check emergency stop flag */
  ret = check_emergency_stop();
  if (ret == k_rx_err_estop) {
    return k_rx_ok; /* E-STOP active, motors already halted */
  }
  if (ret != k_rx_ok) {
    return ret;
  }

  /* 2. Read encoder feedback and update shared state */
  ret = read_encoder_feedback(dt);
  if (ret != k_rx_ok) {
    return ret;
  }

  /* 3. Read motor setpoint from shared state */
  ret = read_motor_setpoint(target_velocities, &setpoint_valid);
  if (ret != k_rx_ok) {
    return ret;
  }

  /* 4. If no valid setpoint, coast motors (0% duty cycle) */
  if (!setpoint_valid) {
    for (uint8_t i = 0; i < k_motor_count; i++) {
      rx_drv8243_set_speed(&s_motor_subsystem.motor_drivers[i], 0.0f);
    }
    return k_rx_ok;
  }

  /* 5. Compute PID and apply duty cycles to motors */
  ret = compute_and_apply_pid(target_velocities, dt);
  if (ret != k_rx_ok) {
    return ret;
  }

  /* 6. Check for motor faults (overcurrent, nFAULT) */
  ret = check_motor_faults();
  if (ret != k_rx_ok) {
    return ret;
  }

  return k_rx_ok;
}
