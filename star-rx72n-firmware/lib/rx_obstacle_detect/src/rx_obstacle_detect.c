/* lib/rx_obstacle_detect/src/rx_obstacle_detect.c */

/**
 * @file rx_obstacle_detect.c
 * @brief Basic obstacle detection with emergency stop implementation
 *
 * @details
 * Implements a ThreadX-based obstacle detection system that polls HC-SR04
 * sensors at a configurable rate and immediately stops all configured motors
 * when an obstacle is detected within the threshold distance.
 *
 * The implementation uses debouncing to avoid false positives and provides
 * event callbacks for obstacle state changes.
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_obstacle_detect.h"

#include <string.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Event flag bit definitions
 */
typedef enum {
  k_event_flag_start = 0x01, /**< Start detection */
  k_event_flag_stop  = 0x02, /**< Stop detection */
} event_flags_t;

/**
 * @brief Validation constants
 */
typedef enum {
  k_min_threshold_cm  = 2,   /**< Minimum detection threshold (HC-SR04 min range) */
  k_max_threshold_cm  = 400, /**< Maximum detection threshold (HC-SR04 max range) */
  k_min_poll_interval = 10,  /**< Minimum poll interval in ms */
  k_max_poll_interval = 1000, /**< Maximum poll interval in ms */
  k_min_debounce      = 1,   /**< Minimum debounce samples */
  k_max_debounce      = 10,  /**< Maximum debounce samples */
} validation_constants_t;

/**
 * @brief ThreadX tick conversion
 */
typedef enum {
  k_ticks_per_second = 100, /**< ThreadX configured for 100 Hz */
} threadx_constants_t;

/* =============================================================================
 * Static Function Declarations
 * =============================================================================
 */

static void    internal_detection_task_entry(ULONG input);
static rx_err_t internal_validate_config(const rx_obstacle_detect_config_t* config);
static rx_err_t internal_stop_all_motors(rx_obstacle_detect_t* handle);
static rx_err_t internal_poll_sensors(rx_obstacle_detect_t* handle);
static void    internal_invoke_callback(rx_obstacle_detect_t* handle,
                                       bool                  obstacle_detected,
                                       uint8_t               sensor_idx,
                                       float                 distance_cm);

/* =============================================================================
 * Public API - Initialization
 * =============================================================================
 */

rx_err_t rx_obstacle_detect_init(rx_obstacle_detect_t*              handle,
                                 const rx_obstacle_detect_config_t* config)
{
  UINT     status      = 0;
  rx_err_t ret         = k_rx_ok;
  uint8_t  i           = 0;

  /* Validate inputs */
  if (handle == NULL || config == NULL) {
    return k_rx_err_null_pointer;
  }

  if (handle->initialized) {
    return k_rx_err_invalid_state;
  }

  ret = internal_validate_config(config);
  if (ret != k_rx_ok) {
    return ret;
  }

  /* Clear handle */
  memset(handle, 0, sizeof(rx_obstacle_detect_t));

  /* Copy configuration */
  handle->sensor_count           = config->sensor_count;
  handle->motor_count            = config->motor_count;
  handle->detection_threshold_cm = config->detection_threshold_cm;
  handle->debounce_samples       = config->debounce_samples;
  handle->poll_interval_ms       = config->poll_interval_ms;
  handle->callback               = config->callback;
  handle->user_data              = config->user_data;

  /* Copy sensor handles */
  for (i = 0; i < config->sensor_count; i++) {
    handle->sensors[i] = config->sensors[i];
  }

  /* Copy motor handles */
  for (i = 0; i < config->motor_count; i++) {
    handle->motors[i] = config->motors[i];
  }

  /* Initialize state */
  handle->state          = k_obstacle_detect_state_stopped;
  handle->stop_requested = false;

  /* Create event flags */
  status = tx_event_flags_create(&handle->event_flags, "ObstacleDetectEvents");
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  /* Create detection task */
  status = tx_thread_create(&handle->thread,
                           "ObstacleDetect",
                           internal_detection_task_entry,
                           (ULONG)handle,
                           handle->thread_stack,
                           k_obstacle_detect_task_stack_size,
                           k_obstacle_detect_task_priority,
                           k_obstacle_detect_task_priority,
                           TX_NO_TIME_SLICE,
                           TX_DONT_START);

  if (status != TX_SUCCESS) {
    tx_event_flags_delete(&handle->event_flags);
    return k_rx_err_rtos_error;
  }

  handle->initialized = true;

  return k_rx_ok;
}

rx_err_t rx_obstacle_detect_deinit(rx_obstacle_detect_t* handle)
{
  UINT     status     = 0;
  rx_err_t stop_ret   = k_rx_ok;

  /* Validate inputs */
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Stop detection if running */
  if (handle->state != k_obstacle_detect_state_stopped) {
    stop_ret = rx_obstacle_detect_stop(handle);
    /* Note: Continue with deinit even if stop fails - we're tearing down anyway */
    /* Caller should check return value if stop failure is critical */
    (void)stop_ret;
  }

  /* Wait for thread to terminate */
  status = tx_thread_terminate(&handle->thread);
  if (status != TX_SUCCESS && status != TX_THREAD_ERROR) {
    return k_rx_err_rtos_error;
  }

  /* Delete thread */
  status = tx_thread_delete(&handle->thread);
  if (status != TX_SUCCESS && status != TX_DELETE_ERROR) {
    return k_rx_err_rtos_error;
  }

  /* Delete event flags */
  status = tx_event_flags_delete(&handle->event_flags);
  if (status != TX_SUCCESS && status != TX_DELETE_ERROR) {
    return k_rx_err_rtos_error;
  }

  /* Clear handle */
  handle->initialized = false;

  return k_rx_ok;
}

/* =============================================================================
 * Public API - Control
 * =============================================================================
 */

rx_err_t rx_obstacle_detect_start(rx_obstacle_detect_t* handle)
{
  UINT status = 0;

  /* Validate inputs */
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Start thread if not already running */
  if (handle->state == k_obstacle_detect_state_stopped) {
    status = tx_thread_resume(&handle->thread);
    if (status != TX_SUCCESS) {
      return k_rx_err_rtos_error;
    }
  }

  /* Signal start event */
  status = tx_event_flags_set(&handle->event_flags, k_event_flag_start, TX_OR);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  return k_rx_ok;
}

rx_err_t rx_obstacle_detect_stop(rx_obstacle_detect_t* handle)
{
  UINT status = 0;

  /* Validate inputs */
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Signal stop event */
  handle->stop_requested = true;
  status = tx_event_flags_set(&handle->event_flags, k_event_flag_stop, TX_OR);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  return k_rx_ok;
}

rx_err_t rx_obstacle_detect_clear_obstacle(rx_obstacle_detect_t* handle)
{
  uint8_t i = 0;

  /* Validate inputs */
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Clear obstacle state */
  for (i = 0; i < handle->sensor_count; i++) {
    handle->debounce_counter[i] = 0;
    handle->obstacle_active[i]  = false;
  }

  /* Update state if currently in obstacle state */
  if (handle->state == k_obstacle_detect_state_obstacle) {
    handle->state = k_obstacle_detect_state_running;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API - Status
 * =============================================================================
 */

rx_err_t rx_obstacle_detect_get_state(const rx_obstacle_detect_t* handle,
                                      rx_obstacle_detect_state_t* out_state)
{
  /* Validate inputs */
  if (handle == NULL || out_state == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  *out_state = handle->state;

  return k_rx_ok;
}

bool rx_obstacle_detect_is_obstacle_detected(const rx_obstacle_detect_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return false;
  }

  return handle->state == k_obstacle_detect_state_obstacle;
}

rx_err_t rx_obstacle_detect_get_stats(const rx_obstacle_detect_t* handle,
                                      uint32_t*                   out_total_polls,
                                      uint32_t*                   out_obstacle_events,
                                      uint32_t*                   out_false_positives)
{
  /* Validate inputs */
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Copy statistics */
  if (out_total_polls != NULL) {
    *out_total_polls = handle->total_polls;
  }

  if (out_obstacle_events != NULL) {
    *out_obstacle_events = handle->obstacle_events;
  }

  if (out_false_positives != NULL) {
    *out_false_positives = handle->false_positive_count;
  }

  return k_rx_ok;
}

rx_err_t rx_obstacle_detect_reset_stats(rx_obstacle_detect_t* handle)
{
  /* Validate inputs */
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Reset statistics */
  handle->total_polls         = 0;
  handle->obstacle_events     = 0;
  handle->false_positive_count = 0;

  return k_rx_ok;
}

/* =============================================================================
 * Static Functions - ThreadX Task
 * =============================================================================
 */

static void internal_detection_task_entry(ULONG input)
{
  rx_obstacle_detect_t* handle         = NULL;
  ULONG                 actual_flags   = 0;
  UINT                  status         = 0;
  ULONG                 sleep_ticks    = 0;
  rx_err_t              ret            = k_rx_ok;
  bool                  running        = false;

  handle = (rx_obstacle_detect_t*)input;

  /* Convert poll interval to ticks */
  sleep_ticks = (handle->poll_interval_ms * k_ticks_per_second) / 1000;
  if (sleep_ticks == 0) {
    sleep_ticks = 1;
  }

  while (true) {
    /* Wait for start event */
    status = tx_event_flags_get(&handle->event_flags,
                               k_event_flag_start,
                               TX_OR_CLEAR,
                               &actual_flags,
                               TX_WAIT_FOREVER);

    if (status != TX_SUCCESS) {
      continue;
    }

    /* Update state */
    handle->state          = k_obstacle_detect_state_running;
    handle->stop_requested = false;
    running                = true;

    /* Detection loop */
    while (running) {
      /* Check for stop event (non-blocking) */
      status = tx_event_flags_get(&handle->event_flags,
                                 k_event_flag_stop,
                                 TX_OR_CLEAR,
                                 &actual_flags,
                                 TX_NO_WAIT);

      if (status == TX_SUCCESS || handle->stop_requested) {
        running               = false;
        handle->state         = k_obstacle_detect_state_stopped;
        handle->stop_requested = false;
        break;
      }

      /* Poll all sensors */
      ret = internal_poll_sensors(handle);
      if (ret != k_rx_ok) {
        /* Critical error during polling (e.g., motor stop failed) */
        /* Stop detection to prevent unsafe operation */
        running               = false;
        handle->state         = k_obstacle_detect_state_stopped;
        handle->stop_requested = false;
        break;
      }

      /* Sleep until next poll */
      tx_thread_sleep(sleep_ticks);
    }
  }
}

/* =============================================================================
 * Static Functions - Validation
 * =============================================================================
 */

static rx_err_t internal_validate_config(const rx_obstacle_detect_config_t* config)
{
  uint8_t i = 0;

  /* Validate sensor configuration */
  if (config->sensors == NULL) {
    return k_rx_err_null_pointer;
  }

  if (config->sensor_count == 0 ||
      config->sensor_count > k_obstacle_detect_max_sensors) {
    return k_rx_err_invalid_arg;
  }

  for (i = 0; i < config->sensor_count; i++) {
    if (config->sensors[i] == NULL) {
      return k_rx_err_null_pointer;
    }
  }

  /* Validate motor configuration */
  if (config->motors == NULL) {
    return k_rx_err_null_pointer;
  }

  if (config->motor_count == 0 ||
      config->motor_count > k_obstacle_detect_max_motors) {
    return k_rx_err_invalid_arg;
  }

  for (i = 0; i < config->motor_count; i++) {
    if (config->motors[i] == NULL) {
      return k_rx_err_null_pointer;
    }
  }

  /* Validate detection parameters */
  if (config->detection_threshold_cm < k_min_threshold_cm ||
      config->detection_threshold_cm > k_max_threshold_cm) {
    return k_rx_err_invalid_arg;
  }

  if (config->debounce_samples < k_min_debounce ||
      config->debounce_samples > k_max_debounce) {
    return k_rx_err_invalid_arg;
  }

  if (config->poll_interval_ms < k_min_poll_interval ||
      config->poll_interval_ms > k_max_poll_interval) {
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Static Functions - Detection Logic
 * =============================================================================
 */

static rx_err_t internal_poll_sensors(rx_obstacle_detect_t* handle)
{
  uint8_t  i                      = 0;
  float    distance_cm            = 0.0f;
  rx_err_t ret                    = k_rx_ok;
  bool     was_obstacle_active    = false;
  bool     is_obstacle_active     = false;

  handle->total_polls++;

  /* Poll each sensor */
  for (i = 0; i < handle->sensor_count; i++) {
    ret = rx_hcsr04_measure_blocking(handle->sensors[i], &distance_cm);

    /* Handle measurement errors */
    if (ret == k_rx_err_timeout) {
      /* No echo = no object (treat as clear) */
      distance_cm = handle->detection_threshold_cm + 1.0f;
    } else if (ret != k_rx_ok) {
      /* Other errors = skip this sensor */
      continue;
    }

    /* Check if obstacle detected */
    if (distance_cm < handle->detection_threshold_cm) {
      /* Increment debounce counter */
      handle->debounce_counter[i]++;

      /* Check if debounce threshold reached */
      if (handle->debounce_counter[i] >= handle->debounce_samples) {
        was_obstacle_active = handle->obstacle_active[i];
        handle->obstacle_active[i] = true;

        /* Fire callback on state change */
        if (!was_obstacle_active) {
          internal_invoke_callback(handle, true, i, distance_cm);
          handle->obstacle_events++;
        }
      }
    } else {
      /* Object beyond threshold - check if state changed */
      if (handle->debounce_counter[i] > 0 &&
          handle->debounce_counter[i] < handle->debounce_samples) {
        /* False positive debounced */
        handle->false_positive_count++;
      }

      /* Reset debounce counter */
      handle->debounce_counter[i] = 0;

      /* Check if obstacle cleared */
      if (handle->obstacle_active[i]) {
        handle->obstacle_active[i] = false;
        internal_invoke_callback(handle, false, i, distance_cm);
      }
    }
  }

  /* Check if any sensor has active obstacle */
  is_obstacle_active = false;
  for (i = 0; i < handle->sensor_count; i++) {
    if (handle->obstacle_active[i]) {
      is_obstacle_active = true;
      break;
    }
  }

  /* Update state and stop motors if needed */
  if (is_obstacle_active && handle->state != k_obstacle_detect_state_obstacle) {
    handle->state = k_obstacle_detect_state_obstacle;
    ret = internal_stop_all_motors(handle);
    if (ret != k_rx_ok) {
      /* CRITICAL: Failed to stop motors during obstacle detection */
      /* This is a safety-critical failure - motors may still be running */
      handle->state = k_obstacle_detect_state_stopped;
      return ret;
    }
  } else if (!is_obstacle_active && handle->state == k_obstacle_detect_state_obstacle) {
    handle->state = k_obstacle_detect_state_running;
  }

  return k_rx_ok;
}

static rx_err_t internal_stop_all_motors(rx_obstacle_detect_t* handle)
{
  uint8_t  i   = 0;
  rx_err_t ret = k_rx_ok;

  for (i = 0; i < handle->motor_count; i++) {
    ret = rx_motor_stop(handle->motors[i], true);
    if (ret != k_rx_ok) {
      /* Log error but continue stopping other motors */
      continue;
    }
  }

  return k_rx_ok;
}

static void internal_invoke_callback(rx_obstacle_detect_t* handle,
                                     bool                  obstacle_detected,
                                     uint8_t               sensor_idx,
                                     float                 distance_cm)
{
  if (handle->callback != NULL) {
    handle->callback(obstacle_detected, sensor_idx, distance_cm, handle->user_data);
  }
}
