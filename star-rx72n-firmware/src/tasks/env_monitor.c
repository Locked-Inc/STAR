/* src/tasks/env_monitor.c */

/**
 * @file env_monitor.c
 * @brief Environment Monitor thread implementation
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/env_monitor.h"

#include <string.h>

#include "env_monitor_config.h"
#include "hardware_pinout.h"
#include "rx_bus_manager.h"
#include "system_config.h"
#include "rx_ds18b20.h"
#include "rx_hcsr04.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_obstacle_detect.h"
#include "shared_state.h"
#include "star_hcsr04_config.h"

static const char* s_tag       = "env_monitor";
static const char* s_task_name = "env_monitor";

static TX_THREAD s_env_monitor_thread;
static uint8_t   s_env_monitor_stack[k_stack_env_monitor];

/* HC-SR04 ultrasonic sensor handles (4 sensors) */
static rx_hcsr04_t s_hcsr04_sensors[k_hcsr04_count];

/* DS18B20 temperature sensor handle */
static rx_ds18b20_handle_t s_ds18b20_handle;

/* Bus manager for 1-Wire communication */
static rx_bus_manager_t s_bus_manager;

/* Temperature sensor state machine */
typedef enum {
  k_temp_state_idle            = 0,
  k_temp_state_conversion_wait = 1,
} temp_state_t;

/* Watchdog timeout constants */
typedef enum {
  k_watchdog_timeout_ms = 60, /**< 3x period = 3 * 20ms = 60ms */
} env_monitor_timeout_constants_t;

static temp_state_t s_temp_state          = k_temp_state_idle;
static uint32_t     s_conversion_start_ms = 0;

static void     env_monitor_entry(ULONG input);
static rx_err_t init_sensors(void);
static rx_err_t init_hcsr04_sensors(void);
static rx_err_t init_ds18b20_sensor(void);
static rx_err_t scan_obstacles(void);
static rx_err_t update_obstacle_safety_state(shared_state_t* state,
                                             bool            obstacle_detected,
                                             float           min_distance,
                                             uint8_t         detecting_sensor);
static rx_err_t monitor_temperature(void);
static rx_err_t handle_temp_idle_state(void);
static rx_err_t handle_temp_wait_state(shared_state_t* state);
static rx_err_t update_temperature_health(shared_state_t* state, float temperature_c);

UINT env_monitor_create(void)
{
  UINT status = tx_thread_create(&s_env_monitor_thread,
                                 s_tag,
                                 env_monitor_entry,
                                 0,
                                 s_env_monitor_stack,
                                 k_stack_env_monitor,
                                 k_priority_env_monitor,
                                 k_priority_env_monitor,
                                 TX_NO_TIME_SLICE,
                                 TX_AUTO_START);

  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Thread creation failed");
    return status;
  }

  rx_log_info(s_tag, "Env_Monitor thread created (priority 5, 50 Hz)");
  return TX_SUCCESS;
}

static void env_monitor_entry(ULONG input)
{
  (void)input;

  rx_log_info(s_tag, "Env_Monitor thread started");

  /* Register with watchdog for task-level monitoring
     * Timeout = 3x period = 3 * 20ms = 60ms */
  rx_err_t ret = rx_iwdt_register_task(s_task_name, k_watchdog_timeout_ms);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Failed to register with watchdog");
  }

  /* Initialize sensors */
  ret = init_sensors();
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Sensor initialization failed");
    /* Cannot continue - halt this thread */
    while (1) {
      tx_thread_sleep(k_threadx_ticks_1s);
    }
  }

  rx_log_info(s_tag, "Sensors initialized, entering main loop");

  /* Main monitoring loop (50 Hz = 20ms period) */
  while (1) {
    scan_obstacles();
    monitor_temperature();

    /* Feed watchdog (critical - prevents reset) */
    rx_iwdt_feed();

    /* Record task heartbeat for deadlock detection */
    rx_iwdt_task_heartbeat(s_task_name);

    tx_thread_sleep(k_threadx_ticks_20ms); /* 20ms = 50 Hz scan rate */
  }
}

/**
 * @brief Initialize HC-SR04 ultrasonic sensors
 *
 * Initializes all 4 HC-SR04 ultrasonic distance sensors for obstacle detection.
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t init_hcsr04_sensors(void)
{
  rx_err_t ret;

  rx_log_info(s_tag, "Initializing HC-SR04 sensors");

  for (uint8_t i = 0; i < k_star_hcsr04_sensor_count; i++) {
    rx_hcsr04_config_t sensor_config;
    ret = star_hcsr04_get_config((star_hcsr04_sensor_id_t)i, &sensor_config);
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "Failed to get HC-SR04 config");
      return ret;
    }

    ret = rx_hcsr04_init(&s_hcsr04_sensors[i], &sensor_config);
    if (ret != k_rx_ok) {
      rx_log_error(s_tag, "HC-SR04 init failed");
      return ret;
    }
  }

  rx_log_info(s_tag, "HC-SR04 sensors initialized");

  return k_rx_ok;
}

/**
 * @brief Initialize DS18B20 temperature sensor
 *
 * Initializes bus manager for 1-Wire communication and configures
 * DS18B20 temperature sensor on P05 (pin 100).
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t init_ds18b20_sensor(void)
{
  rx_err_t ret;

  rx_log_info(s_tag, "Initializing DS18B20 temperature sensor");

  /* Initialize bus manager for 1-Wire */
  ret = rx_bus_manager_init(&s_bus_manager, s_tag, NULL, NULL);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Bus manager init failed");
    return ret;
  }

  /* Create bus configuration node with 1-Wire config in proto union */
  static rx_bus_config_t bus_config = {
    .name              = "onewire0",
    .type              = k_bus_type_onewire,
    .initialized       = false,
    .handle            = NULL,
    .user_ctx          = NULL,
    .proto.onewire.pin = k_gpio_p05, /* P05 (pin 100) */
    .next              = NULL,
  };

  /* Register 1-Wire bus with manager */
  ret = rx_bus_manager_add_bus(&s_bus_manager, &bus_config);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Failed to register 1-Wire bus");
    return ret;
  }

  /* Configure DS18B20 to use bus manager */
  rx_ds18b20_config_t temp_config = {
    .bus_manager      = &s_bus_manager,
    .bus_name         = "onewire0",
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false, /* Single device mode (skip ROM) */
  };

  ret = rx_ds18b20_init(&s_ds18b20_handle, &temp_config);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "DS18B20 init failed");
    return ret;
  }

  rx_log_info(s_tag, "DS18B20 sensor initialized");

  return k_rx_ok;
}

static rx_err_t init_sensors(void)
{
  rx_err_t ret;

  /* Initialize HC-SR04 ultrasonic sensors (4 sensors for obstacle detection) */
  ret = init_hcsr04_sensors();
  if (ret != k_rx_ok) {
    return ret;
  }

  /* Initialize DS18B20 temperature sensor (1-Wire on P05) */
  ret = init_ds18b20_sensor();
  if (ret != k_rx_ok) {
    return ret;
  }

  return k_rx_ok;
}

/**
 * @brief Update safety state with obstacle detection results
 *
 * Updates shared safety state with obstacle detection data and triggers
 * emergency stop if a new obstacle is detected.
 *
 * @param state Shared state pointer (must be non-NULL)
 * @param obstacle_detected Whether obstacle was detected
 * @param min_distance Minimum distance detected (cm)
 * @param detecting_sensor Sensor index that detected obstacle
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t update_obstacle_safety_state(shared_state_t* state,
                                             bool            obstacle_detected,
                                             float           min_distance,
                                             uint8_t         detecting_sensor)
{
  /* Update safety state (mutex-protected) */
  UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire safety mutex");
    return k_rx_err_threadx;
  }

  const bool previous_obstacle = state->safety.obstacle_detected;

  state->safety.obstacle_detected    = obstacle_detected;
  state->safety.obstacle_distance_cm = min_distance;
  state->safety.obstacle_sensor_idx  = detecting_sensor;

  if (obstacle_detected && !previous_obstacle) {
    /* New obstacle detected - trigger E-STOP */
    rx_log_error(s_tag, "Obstacle detected - E-STOP triggered");
    state->safety.emergency_stop = true;
  } else if (!obstacle_detected && previous_obstacle) {
    /* Obstacle cleared */
    rx_log_info(s_tag, "Obstacle cleared");
    /* Note: E-STOP flag remains set until manual clearance */
  }

  tx_mutex_put(&state->safety_mutex);

  return k_rx_ok;
}

static rx_err_t scan_obstacles(void)
{
  /* -------------------------------------------------------------------------
     * Issue 14: Obstacle Detection Integration
     * -------------------------------------------------------------------------
     * Scan all 4 HC-SR04 ultrasonic sensors
     * Check if any obstacle is within threshold (30cm)
     * Update safety state and trigger E-STOP if detected
     */

  shared_state_t* state             = shared_state_get();
  bool            obstacle_detected = false;
  float           min_distance      = k_obstacle_threshold_cm;
  uint8_t         detecting_sensor  = 0;

  /* Scan all sensors for obstacles */
  for (uint8_t i = 0; i < k_star_hcsr04_sensor_count; i++) {
    float    distance_cm = 0.0f;
    rx_err_t ret         = rx_hcsr04_measure_blocking(&s_hcsr04_sensors[i], &distance_cm);

    if (ret != k_rx_ok) {
      /* Sensor read failed - continue to next sensor */
      continue;
    }

    /* Check if obstacle is within threshold */
    if (distance_cm < k_obstacle_threshold_cm) {
      obstacle_detected = true;
      if (distance_cm < min_distance) {
        min_distance     = distance_cm;
        detecting_sensor = i;
      }
    }
  }

  /* Update safety state with results */
  return update_obstacle_safety_state(state, obstacle_detected, min_distance, detecting_sensor);
}

/**
 * @brief Handle temperature state machine IDLE state
 *
 * Triggers DS18B20 temperature conversion and transitions to WAIT state.
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t handle_temp_idle_state(void)
{
  /* Trigger temperature conversion */
  rx_err_t ret = rx_ds18b20_trigger_conversion(&s_ds18b20_handle);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Temperature conversion trigger failed");
    return ret;
  }

  /* Record start time and transition to WAIT state */
  s_conversion_start_ms = tx_time_get();
  s_temp_state          = k_temp_state_conversion_wait;

  return k_rx_ok;
}

/**
 * @brief Update health state with temperature reading
 *
 * Updates health state with temperature, checks thermal thresholds,
 * and triggers E-STOP if shutdown threshold exceeded.
 *
 * @param state Shared state pointer (must be non-NULL)
 * @param temperature_c Temperature in Celsius
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t update_temperature_health(shared_state_t* state, float temperature_c)
{
  /* Update health state (mutex-protected) */
  UINT status = tx_mutex_get(&state->health_mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to acquire health mutex");
    s_temp_state = k_temp_state_idle;
    return k_rx_err_threadx;
  }

  state->health.temperature_celsius = temperature_c;

  /* Check for thermal warning */
  if (temperature_c > k_temp_warning_threshold_c) {
    state->health.thermal_warning = true;
    if (temperature_c > k_temp_shutdown_threshold_c) {
      /* Thermal shutdown - trigger E-STOP */
      rx_log_error(s_tag, "Thermal shutdown - E-STOP triggered");

      status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
      if (status == TX_SUCCESS) {
        state->safety.emergency_stop = true;
        tx_mutex_put(&state->safety_mutex);
      }
    } else {
      rx_log_error(s_tag, "Thermal warning");
    }
  } else {
    state->health.thermal_warning = false;
  }

  tx_mutex_put(&state->health_mutex);

  return k_rx_ok;
}

/**
 * @brief Handle temperature state machine WAIT state
 *
 * Checks if conversion time has elapsed, reads temperature when ready,
 * updates health state, and transitions back to IDLE.
 *
 * @param state Shared state pointer (must be non-NULL)
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t handle_temp_wait_state(shared_state_t* state)
{
  /* Check if conversion time has elapsed */
  const uint32_t elapsed_ms  = tx_time_get() - s_conversion_start_ms;
  const uint32_t required_ms = rx_ds18b20_get_conversion_time_ms(&s_ds18b20_handle);

  if (elapsed_ms < required_ms) {
    /* Conversion still in progress, return immediately */
    return k_rx_ok;
  }

  /* Conversion complete - read temperature */
  float    temperature_c = 0.0f;
  rx_err_t ret           = rx_ds18b20_read_temperature(&s_ds18b20_handle, &temperature_c);
  if (ret != k_rx_ok) {
    rx_log_error(s_tag, "Temperature read failed");
    s_temp_state = k_temp_state_idle; /* Reset to idle on error */
    return ret;
  }

  /* Update health state with temperature */
  ret = update_temperature_health(state, temperature_c);
  if (ret != k_rx_ok) {
    return ret;
  }

  /* Transition back to IDLE for next conversion */
  s_temp_state = k_temp_state_idle;

  return k_rx_ok;
}

static rx_err_t monitor_temperature(void)
{
  /* -------------------------------------------------------------------------
     * Issue 15: Temperature Monitoring Integration (Non-Blocking)
     * -------------------------------------------------------------------------
     * Read DS18B20 temperature sensor (1-Wire) using state machine
     * to avoid blocking the 50 Hz task loop.
     *
     * State machine:
     * - IDLE: Trigger conversion, transition to WAIT
     * - WAIT: Check if conversion time elapsed, read temperature, transition to IDLE
     *
     * This ensures obstacle detection continues at 50 Hz even during
     * temperature conversion (which takes 750ms for 12-bit resolution)
     */

  /* Dispatch to appropriate state handler */
  switch (s_temp_state) {
    case k_temp_state_idle:
      return handle_temp_idle_state();

    case k_temp_state_conversion_wait: {
      shared_state_t* state = shared_state_get();
      return handle_temp_wait_state(state);
    }

    default:
      /* Invalid state - reset to idle */
      s_temp_state = k_temp_state_idle;
      return k_rx_err_invalid_state;
  }
}
