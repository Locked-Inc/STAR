/* lib/rx_hcsr04/src/rx_hcsr04.c */

/**
 * @file rx_hcsr04.c
 * @brief HC-SR04 Ultrasonic Distance Sensor Driver Implementation
 *
 * @details
 * GPIO-based driver for HC-SR04 ultrasonic distance sensors with configurable
 * GPIO pins, timeout handling, and distance measurement in both blocking and
 * asynchronous modes. Supports temperature compensation for speed of sound.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_hcsr04.h"

#include <stddef.h>

#include "rx_check.h"
#include "rx_hcsr04_hal.h"
#include "tx_api.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Worker thread priority (lower number = higher priority)
 */
typedef enum : uint8_t {
  k_worker_priority = 10, /**< Medium priority for sensor measurements */
} rx_hcsr04_worker_priority_t;

/**
 * @brief Worker thread stack size
 */
typedef enum : uint16_t {
  k_worker_stack_size = 1024, /**< 1KB stack for worker thread */
} rx_hcsr04_worker_stack_t;

/**
 * @brief Worker thread event flags
 */
typedef enum : uint8_t {
  k_event_measurement_request = 0x01, /**< Measurement request pending */
  k_event_shutdown_request    = 0x02, /**< Worker shutdown requested */
} rx_hcsr04_event_flags_t;

/**
 * @brief Unit conversion constants
 */
typedef enum : uint16_t {
  k_cm_per_inch_x100 = 254, /**< Centimeters per inch * 100 (2.54 * 100) */
} rx_hcsr04_conversion_t;

/**
 * @brief Scaling factor for integer-based unit conversion
 */
typedef enum : uint8_t {
  k_unit_scale_factor = 100, /**< Scale factor for fixed-point conversion */
} rx_hcsr04_scale_t;

/**
 * @brief Shutdown wait time in ThreadX ticks
 */
typedef enum : uint8_t {
  k_shutdown_wait_ticks = 5, /**< ~50ms at 100 Hz tick rate */
} rx_hcsr04_shutdown_t;

/**
 * @brief Echo polling loop bounds
 */
typedef enum : uint16_t {
  k_echo_poll_max_iterations = 30000, /**< Max polling iterations */
} rx_hcsr04_poll_limits_t;

/**
 * @brief Speed of sound base constant (m/s at 0°C)
 */
static const float s_speed_of_sound_base_mps = 331.3f;

/**
 * @brief Speed of sound temperature coefficient (m/s per °C)
 */
static const float s_speed_of_sound_coeff = 0.606f;

/**
 * @brief Default temperature for distance calculations (°C)
 */
static const float s_default_temperature_celsius = 20.0f;

/**
 * @brief Minimum valid temperature (°C) - DS18B20 sensor lower limit
 */
static const float s_min_temp_celsius = -40.0f;

/**
 * @brief Maximum valid temperature (°C) - DS18B20 sensor upper limit
 */
static const float s_max_temp_celsius = 85.0f;

/**
 * @brief Conversion factor from m/s to cm/us (1 m/s = 0.0001 cm/us)
 */
static const float s_mps_to_cm_per_us = 10000.0F;

/**
 * @brief Roundtrip divisor (echo travels to target and back)
 */
static const float s_roundtrip_divisor = 2.0F;

/* =============================================================================
 * Worker Thread Infrastructure
 * =============================================================================
 */

/**
 * @brief Pending measurement context
 *
 * Stores the context for a pending async measurement request.
 */
typedef struct {
  rx_hcsr04_t*         handle;    /**< Sensor handle */
  rx_hcsr04_callback_t callback;  /**< Completion callback */
  void*                user_data; /**< User context */
} pending_measurement_t;

/**
 * @brief Worker thread handle
 */
static TX_THREAD s_hcsr04_worker_thread;

/**
 * @brief Worker thread stack
 */
static uint8_t s_worker_stack[k_worker_stack_size];

/**
 * @brief Event flags for signaling worker thread
 */
static TX_EVENT_FLAGS_GROUP s_measurement_request;

/**
 * @brief Mutex protecting access to pending measurement context
 */
static TX_MUTEX s_pending_mutex;

/**
 * @brief Pending measurement context
 *
 * Protected by s_pending_mutex. Access only within mutex lock.
 * When handle is NULL, the worker is idle and ready for new requests.
 */
static pending_measurement_t s_pending;

/**
 * @brief Worker thread initialization state
 */
static bool s_worker_initialized = false;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Send 10us trigger pulse
 *
 * @param[in] handle Sensor handle
 */
static rx_err_t internal_send_trigger_pulse(const rx_hcsr04_t* handle)
{
  rx_err_t err;
  uint8_t  port;
  uint8_t  pin_num;

  if (handle == NULL) {
    return k_rx_err_invalid_arg;
  }

  port    = (uint8_t)(handle->trigger_pin >> k_port_shift);
  pin_num = (uint8_t)(handle->trigger_pin & k_port_mask);
  if ((port < k_rx_port_0) || (port > k_rx_port_j) || (port > k_rx_port_g && port < k_rx_port_j) ||
      (pin_num < k_rx_pin_0) || (pin_num > k_rx_pin_max)) {
    return k_rx_err_invalid_arg;
  }

  /* Ensure trigger is low initially */
  err = hcsr04_hal_gpio_write_low(handle->trigger_pin);
  RX_RETURN_ON_ERROR(err, "HCSR04", "Failed to set trigger low");
  hcsr04_hal_delay_us(k_hcsr04_trigger_settle_us);

  /* Send 10us HIGH pulse */
  err = hcsr04_hal_gpio_write_high(handle->trigger_pin);
  RX_RETURN_ON_ERROR(err, "HCSR04", "Failed to set trigger high");
  hcsr04_hal_delay_us(k_hcsr04_trigger_pulse_us);
  err = hcsr04_hal_gpio_write_low(handle->trigger_pin);
  RX_RETURN_ON_ERROR(err, "HCSR04", "Failed to clear trigger low");

  return k_rx_ok;
}

/**
 * @brief Wait for echo pin to reach target state with timeout
 *
 * @param[in,out] handle Sensor handle (cancel_requested may be cleared)
 * @param[in] target_state State to wait for (true=high, false=low)
 * @param[in] timeout_us Timeout in microseconds
 *
 * @return k_rx_ok if state reached
 * @return k_rx_err_timeout if timed out
 * @return k_rx_err_cancelled if operation was cancelled
 */
static rx_err_t
internal_wait_for_echo(rx_hcsr04_t* handle, const bool target_state, uint32_t timeout_us)
{
  const uint32_t start_time = hcsr04_hal_get_time_us();
  uint32_t       elapsed    = 0;
  bool           pin_state  = false;
  rx_err_t       read_err   = k_rx_ok;

  for (uint32_t i = 0; i < k_echo_poll_max_iterations; i++) {
    /* Check for cancellation request */
    if (handle->cancel_requested) {
      handle->cancel_requested = false;
      return k_rx_err_cancelled;
    }

    read_err = hcsr04_hal_gpio_read(handle->echo_pin, &pin_state);
    if (read_err != k_rx_ok) {
      return read_err;
    }

    if (pin_state == target_state) {
      return k_rx_ok;
    }

    /* Check for timeout */
    elapsed = hcsr04_hal_get_time_us() - start_time;
    if (elapsed >= timeout_us) {
      return k_rx_err_timeout;
    }
  }

  return k_rx_err_timeout;
}

/**
 * @brief Measure echo pulse duration
 *
 * @param[in] handle Sensor handle
 * @param[out] duration_us Pulse duration in microseconds
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_measure_echo_pulse(rx_hcsr04_t* handle, uint32_t* duration_us)
{
  rx_err_t err;
  uint32_t pulse_start = 0;
  uint32_t pulse_end   = 0;

  /* Wait for echo to go HIGH (pulse start) */
  err = internal_wait_for_echo(handle, true, handle->timeout_us);
  if (err != k_rx_ok) {
    return err;
  }

  pulse_start = hcsr04_hal_get_time_us();

  /* Wait for echo to go LOW (pulse end) */
  err = internal_wait_for_echo(handle, false, handle->timeout_us);
  if (err != k_rx_ok) {
    return err;
  }

  pulse_end    = hcsr04_hal_get_time_us();
  *duration_us = pulse_end - pulse_start;

  return k_rx_ok;
}

/* =============================================================================
 * Worker Thread Implementation
 * =============================================================================
 */

/**
 * @brief Worker thread entry function
 *
 * Waits for measurement requests or shutdown signal via event flags.
 * Performs measurements and invokes callbacks from worker thread context.
 * Exits gracefully when shutdown is requested.
 *
 * @param[in] input Thread input parameter (unused)
 */
static void hcsr04_worker_entry(const ULONG input)
{
  (void)input;
  ULONG              actual_flags;
  rx_hcsr04_result_t result;
  UINT               status;
  rx_err_t           err;

  while (true) {
    /* Wait for measurement request OR shutdown request */
    status = tx_event_flags_get(&s_measurement_request,
                                k_event_measurement_request | k_event_shutdown_request,
                                TX_OR_CLEAR,
                                &actual_flags,
                                TX_WAIT_FOREVER);

    if (status != TX_SUCCESS) {
      continue; /* Should not happen with TX_WAIT_FOREVER */
    }

    /* Check for shutdown request */
    if (actual_flags & k_event_shutdown_request) {
      break; /* Exit loop gracefully */
    }

    /* Perform blocking measurement */
    err = rx_hcsr04_measure(s_pending.handle, &result);
    if (err != k_rx_ok) {
      result.status = err;
    }

    /* Clear active flag */
    s_pending.handle->measurement_active = false;

    /* Invoke callback from worker context */
    s_pending.callback(s_pending.handle, &result, s_pending.user_data);

    /*
     * Clear pending handle to signal worker is idle and ready for next request.
     * Protected by mutex to prevent race with rx_hcsr04_measure_async().
     */
    tx_mutex_get(&s_pending_mutex, TX_WAIT_FOREVER);
    s_pending.handle = NULL;
    tx_mutex_put(&s_pending_mutex);
  }
}

/* =============================================================================
 * Public API - Worker Thread Management
 * =============================================================================
 */

rx_err_t rx_hcsr04_worker_init(void)
{
  UINT status;

  /* Check if already initialized */
  if (s_worker_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Create mutex for pending measurement protection */
  status = tx_mutex_create(&s_pending_mutex, "HCSR04_Mutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  /* Create event flags group */
  status = tx_event_flags_create(&s_measurement_request, "HCSR04_Events");
  if (status != TX_SUCCESS) {
    tx_mutex_delete(&s_pending_mutex);
    return k_rx_err_rtos_error;
  }

  /* Initialize pending context (worker is idle) */
  s_pending.handle    = NULL;
  s_pending.callback  = NULL;
  s_pending.user_data = NULL;

  /* Create worker thread */
  status = tx_thread_create(&s_hcsr04_worker_thread,
                            "HCSR04_Worker",
                            hcsr04_worker_entry,
                            0,
                            s_worker_stack,
                            k_worker_stack_size,
                            k_worker_priority,
                            k_worker_priority,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    /* Cleanup on failure */
    tx_event_flags_delete(&s_measurement_request);
    tx_mutex_delete(&s_pending_mutex);
    return k_rx_err_rtos_error;
  }

  s_worker_initialized = true;
  return k_rx_ok;
}

rx_err_t rx_hcsr04_worker_deinit(void)
{
  UINT status;

  /* Check if initialized */
  if (!s_worker_initialized) {
    return k_rx_err_invalid_state;
  }

  /*
   * Graceful shutdown: Signal worker thread to exit via event flag.
   * This prevents abrupt termination mid-measurement, which could leave
   * sensor handles stuck in measurement_active state.
   */
  status = tx_event_flags_set(&s_measurement_request, k_event_shutdown_request, TX_OR);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  /*
   * Wait for worker thread to exit gracefully.
   * In production, we'd use a semaphore or check thread state.
   * For simplicity, we assume the worker exits quickly (< 30ms measurement).
   */
  status = tx_thread_sleep(k_shutdown_wait_ticks);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  /* Delete thread (now safely terminated) */
  status = tx_thread_delete(&s_hcsr04_worker_thread);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  /* Delete event flags */
  status = tx_event_flags_delete(&s_measurement_request);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  /* Delete mutex */
  status = tx_mutex_delete(&s_pending_mutex);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_error;
  }

  s_worker_initialized = false;
  return k_rx_ok;
}

/* =============================================================================
 * Public API - Initialization
 * =============================================================================
 */

rx_err_t rx_hcsr04_init(rx_hcsr04_t* handle, const rx_hcsr04_config_t* config)
{
  rx_err_t err;

  if (handle == NULL || config == NULL) {
    return k_rx_err_null_ptr;
  }

  if (handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Configure trigger pin as output */
  err = hcsr04_hal_gpio_set_output(config->trigger_pin);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure echo pin as input */
  err = hcsr04_hal_gpio_set_input(config->echo_pin);
  if (err != k_rx_ok) {
    /* Cleanup trigger pin */
    hcsr04_hal_gpio_deinit(config->trigger_pin);
    return err;
  }

  /* Initialize handle */
  handle->trigger_pin               = config->trigger_pin;
  handle->echo_pin                  = config->echo_pin;
  handle->timeout_us                = config->timeout_us;
  handle->initialized               = true;
  handle->measurement_active        = false;
  handle->cancel_requested          = false;
  handle->temperature_celsius       = s_default_temperature_celsius;
  handle->temp_compensation_enabled = false;

  /* Reset statistics */
  handle->measurement_count = 0;
  handle->timeout_count     = 0;
  handle->range_error_count = 0;

  /* Ensure trigger is low */
  err = hcsr04_hal_gpio_write_low(handle->trigger_pin);
  if (err != k_rx_ok) {
    return err;
  }

  return k_rx_ok;
}

rx_err_t rx_hcsr04_deinit(rx_hcsr04_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Release GPIO pins */
  hcsr04_hal_gpio_deinit(handle->trigger_pin);
  hcsr04_hal_gpio_deinit(handle->echo_pin);

  /* Clear handle */
  handle->initialized = false;

  return k_rx_ok;
}

/* =============================================================================
 * Public API - Measurement
 * =============================================================================
 */

rx_err_t rx_hcsr04_measure_blocking(rx_hcsr04_t* handle, float* distance_cm)
{
  rx_err_t err;
  uint32_t echo_time_us = 0;

  if (handle == NULL || distance_cm == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Increment measurement count */
  handle->measurement_count++;

  /* Send trigger pulse */
  err = internal_send_trigger_pulse(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Measure echo pulse duration */
  err = internal_measure_echo_pulse(handle, &echo_time_us);

  if (err == k_rx_err_timeout) {
    handle->timeout_count++;
    return k_rx_err_timeout;
  }

  if (err != k_rx_ok) {
    return err;
  }

  /* Convert to distance (with temperature compensation if enabled) */
  if (handle->temp_compensation_enabled) {
    *distance_cm = rx_hcsr04_echo_to_cm_with_temp(echo_time_us, handle->temperature_celsius);
  } else {
    *distance_cm = rx_hcsr04_echo_to_cm(echo_time_us);
  }

  /* Validate range */
  if (*distance_cm < (float)k_hcsr04_min_distance_cm ||
      *distance_cm > (float)k_hcsr04_max_distance_cm) {
    handle->range_error_count++;
    return k_rx_err_out_of_range;
  }

  return k_rx_ok;
}

rx_err_t rx_hcsr04_measure(rx_hcsr04_t* handle, rx_hcsr04_result_t* result)
{
  rx_err_t err;

  if (handle == NULL || result == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Initialize result */
  result->distance_cm  = 0.0f;
  result->distance_in  = 0.0f;
  result->echo_time_us = 0;
  result->status       = k_rx_ok;

  /* Increment measurement count */
  handle->measurement_count++;

  /* Send trigger pulse */
  err = internal_send_trigger_pulse(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Measure echo pulse duration */
  err = internal_measure_echo_pulse(handle, &result->echo_time_us);

  if (err == k_rx_err_timeout) {
    handle->timeout_count++;
    result->status = k_rx_err_timeout;
    return k_rx_err_timeout;
  }

  if (err != k_rx_ok) {
    result->status = err;
    return err;
  }

  /* Convert to distance (with temperature compensation if enabled) */
  if (handle->temp_compensation_enabled) {
    result->distance_cm =
      rx_hcsr04_echo_to_cm_with_temp(result->echo_time_us, handle->temperature_celsius);
  } else {
    result->distance_cm = rx_hcsr04_echo_to_cm(result->echo_time_us);
  }
  result->distance_in = rx_hcsr04_cm_to_inches(result->distance_cm);
  result->status      = k_rx_ok;

  /* Validate range */
  if (result->distance_cm < (float)k_hcsr04_min_distance_cm ||
      result->distance_cm > (float)k_hcsr04_max_distance_cm) {
    handle->range_error_count++;
    result->status = k_rx_err_out_of_range;
    return k_rx_err_out_of_range;
  }

  return k_rx_ok;
}

rx_err_t
rx_hcsr04_measure_async(rx_hcsr04_t* handle, const rx_hcsr04_callback_t callback, void* user_data)
{
  rx_err_t           err;
  UINT               status;
  rx_hcsr04_result_t result;

  if (handle == NULL || callback == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (handle->measurement_active) {
    return k_rx_err_busy;
  }

  handle->measurement_active = true;

  /* Check if worker thread is initialized */
  if (s_worker_initialized) {
    /*
     * True async mode: queue measurement request for worker thread.
     * Use mutex to prevent race condition with worker thread.
     */
    tx_mutex_get(&s_pending_mutex, TX_WAIT_FOREVER);

    /* Check if worker is already busy with another sensor */
    if (s_pending.handle != NULL) {
      tx_mutex_put(&s_pending_mutex);
      handle->measurement_active = false;
      return k_rx_err_busy;
    }

    /* Queue this measurement */
    s_pending.handle    = handle;
    s_pending.callback  = callback;
    s_pending.user_data = user_data;

    tx_mutex_put(&s_pending_mutex);

    /* Signal worker thread - function returns immediately */
    status = tx_event_flags_set(&s_measurement_request, k_event_measurement_request, TX_OR);

    if (status != TX_SUCCESS) {
      /* Rollback on failure */
      tx_mutex_get(&s_pending_mutex, TX_WAIT_FOREVER);
      s_pending.handle = NULL;
      tx_mutex_put(&s_pending_mutex);
      handle->measurement_active = false;
      return k_rx_err_rtos_error;
    }

    return k_rx_ok; /* Callback will be invoked from worker thread */
  }
  /* Fallback sync mode: perform measurement inline (backward compatible) */
  err = rx_hcsr04_measure(handle, &result);

  handle->measurement_active = false;

  /* Invoke callback before return (synchronous) */
  callback(handle, &result, user_data);

  return err;
}

bool rx_hcsr04_is_busy(const rx_hcsr04_t* handle)
{
  if (handle == NULL) {
    return false;
  }

  return handle->measurement_active;
}

rx_err_t rx_hcsr04_cancel(rx_hcsr04_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->measurement_active) {
    return k_rx_err_invalid_state;
  }

  /*
   * Set cancel flag. The worker thread (or sync fallback) will check
   * this flag in internal_wait_for_echo() and abort the measurement.
   */
  handle->cancel_requested = true;

  return k_rx_ok;
}

/* =============================================================================
 * Public API - Temperature Compensation
 * =============================================================================
 */

rx_err_t rx_hcsr04_set_temperature(rx_hcsr04_t* handle, const float temp_celsius)
{
  if (handle == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Validate temperature range */
  if (temp_celsius < s_min_temp_celsius || temp_celsius > s_max_temp_celsius) {
    return k_rx_err_invalid_arg;
  }

  /* Update temperature and enable compensation */
  handle->temperature_celsius       = temp_celsius;
  handle->temp_compensation_enabled = true;

  return k_rx_ok;
}

rx_err_t rx_hcsr04_disable_temp_compensation(rx_hcsr04_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  handle->temp_compensation_enabled = false;

  return k_rx_ok;
}

bool rx_hcsr04_is_temp_compensation_enabled(const rx_hcsr04_t* handle)
{
  if (handle == NULL) {
    return false;
  }

  return handle->temp_compensation_enabled;
}

rx_err_t rx_hcsr04_get_temperature(const rx_hcsr04_t* handle, float* temp_celsius)
{
  if (handle == NULL || temp_celsius == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  *temp_celsius = handle->temperature_celsius;

  return k_rx_ok;
}

/* =============================================================================
 * Public API - Utilities
 * =============================================================================
 */

float rx_hcsr04_cm_to_inches(const float distance_cm)
{
  /* 1 inch = 2.54 cm */
  return distance_cm * (float)k_unit_scale_factor / (float)k_cm_per_inch_x100;
}

float rx_hcsr04_echo_to_cm(const uint32_t echo_time_us)
{
  /*
   * Speed of sound at 20C = 343 m/s = 0.0343 cm/us
   * Distance = (time_us * 0.0343) / 2 (roundtrip)
   * Distance = time_us / 58.3
   *
   * Using integer constant for precision.
   */
  return (float)echo_time_us / (float)k_hcsr04_us_per_cm_roundtrip;
}

float rx_hcsr04_get_speed_of_sound(float temp_celsius)
{
  /*
   * Speed of sound in dry air:
   * v = 331.3 + (0.606 * temp_c) m/s
   *
   * Valid range: -40°C to +85°C (DS18B20 sensor range)
   */

  /* Clamp temperature to valid range */
  if (temp_celsius < s_min_temp_celsius) {
    temp_celsius = s_min_temp_celsius;
  } else if (temp_celsius > s_max_temp_celsius) {
    temp_celsius = s_max_temp_celsius;
  }

  return s_speed_of_sound_base_mps + (s_speed_of_sound_coeff * temp_celsius);
}

float rx_hcsr04_echo_to_cm_with_temp(const uint32_t echo_time_us, float temp_celsius)
{
  float speed_mps   = 0.0F;
  float speed_cm_us = 0.0F;
  float distance_cm = 0.0F;
  /*
   * Temperature-compensated distance calculation:
   * 1. Calculate speed of sound at given temperature
   * 2. Convert speed to cm/us: speed_cm_us = speed_mps / 10000
   * 3. Calculate distance: distance = (echo_us * speed_cm_us) / 2
   *
   * Example at 20°C:
   * - Speed = 331.3 + (0.606 * 20) = 343.42 m/s
   * - Speed = 0.034342 cm/us
   * - For echo_us = 580: distance = (580 * 0.034342) / 2 = 9.96 cm ≈ 10 cm
   */

  /* Pre-condition: Validate temperature range - use default if invalid */
  if (temp_celsius < s_min_temp_celsius || temp_celsius > s_max_temp_celsius) {
    return rx_hcsr04_echo_to_cm(echo_time_us);
  }

  /* Pre-condition: Validate echo time */
  if (echo_time_us == 0 || echo_time_us > k_hcsr04_echo_timeout_us) {
    return 0.0f;
  }

  speed_mps   = rx_hcsr04_get_speed_of_sound(temp_celsius);
  speed_cm_us = speed_mps / s_mps_to_cm_per_us;
  distance_cm = ((float)echo_time_us * speed_cm_us) / s_roundtrip_divisor;

  /* Post-condition: Ensure non-negative result */
  if (distance_cm < 0.0f) {
    return 0.0f;
  }

  return distance_cm;
}

rx_err_t rx_hcsr04_get_stats(const rx_hcsr04_t* handle,
                             uint32_t*          measurement_count,
                             uint32_t*          timeout_count,
                             uint32_t*          range_error_count)
{
  if (handle == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (measurement_count != NULL) {
    *measurement_count = handle->measurement_count;
  }

  if (timeout_count != NULL) {
    *timeout_count = handle->timeout_count;
  }

  if (range_error_count != NULL) {
    *range_error_count = handle->range_error_count;
  }

  return k_rx_ok;
}

rx_err_t rx_hcsr04_reset_stats(rx_hcsr04_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_null_ptr;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  handle->measurement_count = 0;
  handle->timeout_count     = 0;
  handle->range_error_count = 0;

  return k_rx_ok;
}
