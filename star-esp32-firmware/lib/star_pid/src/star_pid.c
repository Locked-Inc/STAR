/* lib/star_pid/src/star_pid.c */

#include "star_pid.h"

#include "esp_check.h"
#include "esp_log.h"

#include <math.h>
#include <string.h>

/* --- Constants --- */

static const char* s_TAG = "STAR_PID";

/* --- Private Function Prototypes --- */

static inline float internal_clamp(float value, float min, float max);

/* --- Public Functions --- */

esp_err_t star_pid_init(star_pid_handle_t* handle, const star_pid_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Already initialized");

  /* Validate configuration */
  ESP_RETURN_ON_FALSE(config->output_max > config->output_min,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "output_max must be > output_min");
  ESP_RETURN_ON_FALSE(config->integral_max > config->integral_min,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "integral_max must be > integral_min");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_pid_handle_t));

  /* Copy configuration */
  handle->kp            = config->kp;
  handle->ki            = config->ki;
  handle->kd            = config->kd;
  handle->output_min    = config->output_min;
  handle->output_max    = config->output_max;
  handle->integral_min  = config->integral_min;
  handle->integral_max  = config->integral_max;
  handle->integral      = 0.0f;
  handle->prev_error    = 0.0f;
  handle->initialized   = true;

  ESP_LOGI(s_TAG,
           "PID initialized: Kp=%.3f, Ki=%.3f, Kd=%.3f, out=[%.1f,%.1f]",
           config->kp,
           config->ki,
           config->kd,
           config->output_min,
           config->output_max);

  return ESP_OK;
}

esp_err_t star_pid_deinit(star_pid_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Clear handle */
  memset(handle, 0, sizeof(star_pid_handle_t));

  ESP_LOGI(s_TAG, "PID deinitialized");
  return ESP_OK;
}

esp_err_t star_pid_compute(star_pid_handle_t* handle,
                            float              setpoint,
                            float              measured,
                            float              dt,
                            float*             output)
{
  ESP_RETURN_ON_FALSE(handle && output, ESP_ERR_INVALID_ARG, s_TAG, "Handle or output is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");
  ESP_RETURN_ON_FALSE(dt > 0.0f, ESP_ERR_INVALID_ARG, s_TAG, "dt must be > 0");

  /* Calculate error */
  float error = setpoint - measured;

  /* Proportional term */
  float p_term = handle->kp * error;

  /* Integral term with anti-windup */
  handle->integral += error * dt;
  handle->integral = internal_clamp(handle->integral, handle->integral_min, handle->integral_max);
  float i_term     = handle->ki * handle->integral;

  /* Derivative term */
  float derivative = (error - handle->prev_error) / dt;
  float d_term     = handle->kd * derivative;

  /* Compute total output */
  float raw_output = p_term + i_term + d_term;

  /* Clamp output to limits */
  *output = internal_clamp(raw_output, handle->output_min, handle->output_max);

  /* Store error for next iteration */
  handle->prev_error = error;

  ESP_LOGD(s_TAG,
           "PID: error=%.2f, P=%.2f, I=%.2f, D=%.2f, out=%.2f",
           error,
           p_term,
           i_term,
           d_term,
           *output);

  return ESP_OK;
}

esp_err_t star_pid_reset(star_pid_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Clear internal state */
  handle->integral   = 0.0f;
  handle->prev_error = 0.0f;

  ESP_LOGD(s_TAG, "PID state reset");
  return ESP_OK;
}

esp_err_t star_pid_set_gains(star_pid_handle_t* handle, float kp, float ki, float kd)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  handle->kp = kp;
  handle->ki = ki;
  handle->kd = kd;

  ESP_LOGI(s_TAG, "PID gains updated: Kp=%.3f, Ki=%.3f, Kd=%.3f", kp, ki, kd);
  return ESP_OK;
}

esp_err_t star_pid_set_output_limits(star_pid_handle_t* handle, float output_min, float output_max)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");
  ESP_RETURN_ON_FALSE(output_max > output_min,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "output_max must be > output_min");

  handle->output_min = output_min;
  handle->output_max = output_max;

  ESP_LOGI(s_TAG, "PID output limits updated: [%.1f, %.1f]", output_min, output_max);
  return ESP_OK;
}

esp_err_t star_pid_set_integral_limits(star_pid_handle_t* handle,
                                        float              integral_min,
                                        float              integral_max)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");
  ESP_RETURN_ON_FALSE(integral_max > integral_min,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "integral_max must be > integral_min");

  handle->integral_min = integral_min;
  handle->integral_max = integral_max;

  /* Clamp current integral to new limits */
  handle->integral = internal_clamp(handle->integral, integral_min, integral_max);

  ESP_LOGI(s_TAG, "PID integral limits updated: [%.1f, %.1f]", integral_min, integral_max);
  return ESP_OK;
}

/* --- Private Functions --- */

static inline float internal_clamp(float value, float min, float max)
{
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}
