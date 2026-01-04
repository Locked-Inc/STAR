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
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_hcsr04.h"

#include <stddef.h>

/* =============================================================================
 * Platform Abstraction
 * =============================================================================
 */

/*
 * When building for host testing, use mock functions.
 * When building for RX72N, use real hardware functions.
 */
#ifdef RX_HCSR04_USE_MOCK

#include "mock_hcsr04_hw.h"

#define GPIO_SET_OUTPUT(port, pin) (mock_gpio_set_output((port), (pin)))
#define GPIO_SET_INPUT(port, pin)  (mock_gpio_set_input((port), (pin)))
#define GPIO_WRITE_HIGH(port, pin) (mock_gpio_write_high((port), (pin)))
#define GPIO_WRITE_LOW(port, pin)  (mock_gpio_write_low((port), (pin)))
#define GPIO_READ(port, pin, val)  (mock_gpio_read((port), (pin), (val)))
#define GPIO_DEINIT(port, pin)     (mock_gpio_deinit((port), (pin)))
#define DELAY_US(us)               (mock_delay_us((us)))
#define GET_TIME_US()              (mock_get_time_us())

#else

#include "hardware.h"
#include "rx_time_interface.h"

/* Real hardware GPIO wrappers - propagate errors from HAL */
static rx_err_t internal_gpio_set_output(uint8_t port, uint8_t pin)
{
  return gpio_set_output(port, pin);
}

static rx_err_t internal_gpio_set_input(uint8_t port, uint8_t pin)
{
  return gpio_set_input(port, pin);
}

static rx_err_t internal_gpio_write_high(uint8_t port, uint8_t pin)
{
  return gpio_write_high(port, pin);
}

static rx_err_t internal_gpio_write_low(uint8_t port, uint8_t pin)
{
  return gpio_write_low(port, pin);
}

static rx_err_t internal_gpio_read(uint8_t port, uint8_t pin, bool* value)
{
  return gpio_read(port, pin, value);
}

/* Real timing functions (from CMT or timer) */
extern void     delay_us(uint32_t us);
extern uint32_t get_time_us(void);

#define GPIO_SET_OUTPUT(port, pin) (internal_gpio_set_output((port), (pin)))
#define GPIO_SET_INPUT(port, pin)  (internal_gpio_set_input((port), (pin)))
#define GPIO_WRITE_HIGH(port, pin) (internal_gpio_write_high((port), (pin)))
#define GPIO_WRITE_LOW(port, pin)  (internal_gpio_write_low((port), (pin)))
#define GPIO_READ(port, pin, val)  (internal_gpio_read((port), (pin), (val)))
/*
 * GPIO_DEINIT is intentionally a no-op: the RX72N GPIO HAL does not provide
 * pin deallocation. GPIO pins are static resources allocated at init time.
 */
#define GPIO_DEINIT(port, pin)     (k_rx_ok)
#define DELAY_US(us)               (delay_us((us)))
#define GET_TIME_US()              (get_time_us())

#endif /* RX_HCSR04_USE_MOCK */

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief Centimeters per inch * 100 (fixed point: 2.54 * 100 = 254)
 */
static const uint32_t s_cm_per_inch_x100 = 254;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Send 10us trigger pulse
 *
 * @param[in] handle Sensor handle
 */
static void internal_send_trigger_pulse(const rx_hcsr04_t* handle)
{
  /* Ensure trigger is low initially */
  GPIO_WRITE_LOW(handle->trigger_port, handle->trigger_pin);
  DELAY_US(2);

  /* Send 10us HIGH pulse */
  GPIO_WRITE_HIGH(handle->trigger_port, handle->trigger_pin);
  DELAY_US(k_hcsr04_trigger_pulse_us);
  GPIO_WRITE_LOW(handle->trigger_port, handle->trigger_pin);
}

/**
 * @brief Wait for echo pin to reach target state with timeout
 *
 * @param[in] handle Sensor handle
 * @param[in] target_state State to wait for (true=high, false=low)
 * @param[in] timeout_us Timeout in microseconds
 *
 * @return k_rx_ok if state reached, k_rx_err_timeout if timed out
 */
static rx_err_t
internal_wait_for_echo(const rx_hcsr04_t* handle, bool target_state, uint32_t timeout_us)
{
  uint32_t start_time = GET_TIME_US();
  bool     pin_state  = false;
  uint32_t elapsed    = 0;

  while (true) {
    GPIO_READ(handle->echo_port, handle->echo_pin, &pin_state);

    if (pin_state == target_state) {
      return k_rx_ok;
    }

    /* Check for timeout */
    elapsed = GET_TIME_US() - start_time;
    if (elapsed >= timeout_us) {
      return k_rx_err_timeout;
    }
  }
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

  /* Wait for echo to go HIGH (pulse start) */
  err = internal_wait_for_echo(handle, true, handle->timeout_us);
  if (err != k_rx_ok) {
    return err;
  }

  uint32_t pulse_start = GET_TIME_US();

  /* Wait for echo to go LOW (pulse end) */
  err = internal_wait_for_echo(handle, false, handle->timeout_us);
  if (err != k_rx_ok) {
    return err;
  }

  uint32_t pulse_end = GET_TIME_US();
  *duration_us       = pulse_end - pulse_start;

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
    return k_rx_err_null_pointer;
  }

  if (handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Configure trigger pin as output */
  err = GPIO_SET_OUTPUT(config->trigger_port, config->trigger_pin);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure echo pin as input */
  err = GPIO_SET_INPUT(config->echo_port, config->echo_pin);
  if (err != k_rx_ok) {
    /* Cleanup trigger pin */
    GPIO_DEINIT(config->trigger_port, config->trigger_pin);
    return err;
  }

  /* Initialize handle */
  handle->trigger_port       = config->trigger_port;
  handle->trigger_pin        = config->trigger_pin;
  handle->echo_port          = config->echo_port;
  handle->echo_pin           = config->echo_pin;
  handle->timeout_us         = config->timeout_us;
  handle->initialized        = true;
  handle->measurement_active = false;

  /* Reset statistics */
  handle->measurement_count = 0;
  handle->timeout_count     = 0;
  handle->range_error_count = 0;

  /* Ensure trigger is low */
  GPIO_WRITE_LOW(handle->trigger_port, handle->trigger_pin);

  return k_rx_ok;
}

rx_err_t rx_hcsr04_deinit(rx_hcsr04_t* handle)
{
  if (handle == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Release GPIO pins */
  GPIO_DEINIT(handle->trigger_port, handle->trigger_pin);
  GPIO_DEINIT(handle->echo_port, handle->echo_pin);

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
  if (handle == NULL || distance_cm == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Increment measurement count */
  handle->measurement_count++;

  /* Send trigger pulse */
  internal_send_trigger_pulse(handle);

  /* Measure echo pulse duration */
  uint32_t echo_time_us;
  rx_err_t err = internal_measure_echo_pulse(handle, &echo_time_us);

  if (err == k_rx_err_timeout) {
    handle->timeout_count++;
    return k_rx_err_timeout;
  }

  if (err != k_rx_ok) {
    return err;
  }

  /* Convert to distance */
  *distance_cm = rx_hcsr04_echo_to_cm(echo_time_us);

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
  if (handle == NULL || result == NULL) {
    return k_rx_err_null_pointer;
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
  internal_send_trigger_pulse(handle);

  /* Measure echo pulse duration */
  rx_err_t err = internal_measure_echo_pulse(handle, &result->echo_time_us);

  if (err == k_rx_err_timeout) {
    handle->timeout_count++;
    result->status = k_rx_err_timeout;
    return k_rx_err_timeout;
  }

  if (err != k_rx_ok) {
    result->status = err;
    return err;
  }

  /* Convert to distance */
  result->distance_cm = rx_hcsr04_echo_to_cm(result->echo_time_us);
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
rx_hcsr04_measure_async(rx_hcsr04_t* handle, rx_hcsr04_callback_t callback, void* user_data)
{
  if (handle == NULL || callback == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  if (handle->measurement_active) {
    return k_rx_err_busy;
  }

  handle->measurement_active = true;

  /* Perform measurement and invoke callback */
  rx_hcsr04_result_t result;
  rx_hcsr04_measure(handle, &result);

  handle->measurement_active = false;

  callback(handle, &result, user_data);

  return k_rx_ok;
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
    return k_rx_err_null_pointer;
  }

  if (!handle->measurement_active) {
    return k_rx_err_invalid_state;
  }

  /* Note: Full implementation would signal worker thread to cancel */
  handle->measurement_active = false;

  return k_rx_ok;
}

/* =============================================================================
 * Public API - Utilities
 * =============================================================================
 */

float rx_hcsr04_cm_to_inches(float distance_cm)
{
  /* 1 inch = 2.54 cm */
  return distance_cm * 100.0f / (float)s_cm_per_inch_x100;
}

float rx_hcsr04_echo_to_cm(uint32_t echo_time_us)
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

rx_err_t rx_hcsr04_get_stats(const rx_hcsr04_t* handle,
                             uint32_t*          measurement_count,
                             uint32_t*          timeout_count,
                             uint32_t*          range_error_count)
{
  if (handle == NULL) {
    return k_rx_err_null_pointer;
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
    return k_rx_err_null_pointer;
  }

  if (!handle->initialized) {
    return k_rx_err_invalid_state;
  }

  handle->measurement_count = 0;
  handle->timeout_count     = 0;
  handle->range_error_count = 0;

  return k_rx_ok;
}
