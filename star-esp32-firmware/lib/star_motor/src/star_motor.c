/**
 * @file star_motor.c
 * @brief Brushed DC motor control implementation using ESP32 MCPWM peripheral
 * @details
 * Implements H-bridge motor control using the ESP32 Motor Control PWM (MCPWM) peripheral.
 * Supports bidirectional control, configurable PWM frequency, dead-time insertion, and
 * fault detection for safe motor operation.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "star_motor.h"

#include <math.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

/* --- Constants --- */

static const char* s_tag = "STAR_MOTOR";

enum {
  k_motor_duty_max      = 100,  /**< Maximum duty cycle percentage */
  k_motor_duty_min      = -100, /**< Minimum duty cycle percentage */
  k_motor_percent_scale = 100,  /**< Percentage scale factor */
  k_motor_duty_zero     = 0,    /**< Zero duty cycle */
};

static const uint64_t s_ns_per_sec = 1000000000ULL; /**< Nanoseconds per second */

/* --- Private Function Prototypes --- */

static inline float internal_clamp_duty(float duty);
static esp_err_t    internal_configure_generators(star_motor_handle_t* handle, bool invert_pwm);

/* --- Public Functions --- */

esp_err_t star_motor_init(star_motor_handle_t* handle, const star_motor_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Already initialized");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_motor_handle_t));

  esp_err_t ret = ESP_OK;

  /* Calculate period in timer ticks */
  uint32_t period_ticks = config->timer_resolution_hz / config->pwm_freq_hz;

  /* Create MCPWM timer */
  mcpwm_timer_config_t timer_config = {
    .group_id      = config->group_id,
    .clk_src       = MCPWM_TIMER_CLK_SRC_DEFAULT,
    .resolution_hz = config->timer_resolution_hz,
    .count_mode    = MCPWM_TIMER_COUNT_MODE_UP_DOWN, /* Center-aligned PWM */
    .period_ticks  = period_ticks,
  };
  ret = mcpwm_new_timer(&timer_config, &handle->timer);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to create MCPWM timer");

  /* Create MCPWM operator */
  mcpwm_operator_config_t operator_config = {
    .group_id = config->group_id,
  };
  ret = mcpwm_new_operator(&operator_config, &handle->operator);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create MCPWM operator: %s", esp_err_to_name(ret));
    mcpwm_del_timer(handle->timer);
    return ret;
  }

  /* Connect operator to timer */
  ret = mcpwm_operator_connect_timer(handle->operator, handle->timer);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to connect operator to timer: %s", esp_err_to_name(ret));
    mcpwm_del_operator(handle->operator);
    mcpwm_del_timer(handle->timer);
    return ret;
  }

  /* Create comparator */
  mcpwm_comparator_config_t comparator_config = {
    .flags.update_cmp_on_tez = true, /* Update on timer equal zero */
  };
  ret = mcpwm_new_comparator(handle->operator, &comparator_config, &handle->comparator);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create comparator: %s", esp_err_to_name(ret));
    mcpwm_del_operator(handle->operator);
    mcpwm_del_timer(handle->timer);
    return ret;
  }

  /* Create generators for PWM A and B */
  mcpwm_generator_config_t gen_a_config = {
    .gen_gpio_num = config->pin_pwm_a,
  };
  ret = mcpwm_new_generator(handle->operator, &gen_a_config, &handle->gen_a);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create generator A: %s", esp_err_to_name(ret));
    mcpwm_del_comparator(handle->comparator);
    mcpwm_del_operator(handle->operator);
    mcpwm_del_timer(handle->timer);
    return ret;
  }

  mcpwm_generator_config_t gen_b_config = {
    .gen_gpio_num = config->pin_pwm_b,
  };
  ret = mcpwm_new_generator(handle->operator, &gen_b_config, &handle->gen_b);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create generator B: %s", esp_err_to_name(ret));
    mcpwm_del_generator(handle->gen_a);
    mcpwm_del_comparator(handle->comparator);
    mcpwm_del_operator(handle->operator);
    mcpwm_del_timer(handle->timer);
    return ret;
  }

  /* Configure dead-time */
  if (config->dead_time_ns > 0) {
    /* Convert nanoseconds to timer ticks */
    uint32_t dead_time_ticks =
      (uint32_t)((config->dead_time_ns * config->timer_resolution_hz) / s_ns_per_sec);

    mcpwm_dead_time_config_t dt_config = {
      .posedge_delay_ticks = dead_time_ticks,
      .negedge_delay_ticks = dead_time_ticks,
      .flags.invert_output = false,
    };

    ret = mcpwm_generator_set_dead_time(handle->gen_a, handle->gen_a, &dt_config);
    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to set dead-time for gen_a: %s", esp_err_to_name(ret));
    }

    ret = mcpwm_generator_set_dead_time(handle->gen_b, handle->gen_b, &dt_config);
    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to set dead-time for gen_b: %s", esp_err_to_name(ret));
    }
  }

  /* Configure generator actions */
  ret = internal_configure_generators(handle, config->invert_pwm);
  if (ret != ESP_OK) {
    mcpwm_del_generator(handle->gen_b);
    mcpwm_del_generator(handle->gen_a);
    mcpwm_del_comparator(handle->comparator);
    mcpwm_del_operator(handle->operator);
    mcpwm_del_timer(handle->timer);
    return ret;
  }

  /* Configure fault detection if pin provided */
  if (config->fault_pin >= 0) {
    mcpwm_gpio_fault_config_t fault_config = {
      .group_id           = config->group_id,
      .gpio_num           = config->fault_pin,
      .flags.pull_up      = true,
      .flags.active_level = 0, /* Active low */
    };
    ret = mcpwm_new_gpio_fault(&fault_config, &handle->fault);
    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to configure fault detection: %s", esp_err_to_name(ret));
      handle->fault = NULL;
    }
  }

  /* Enable and start timer */
  ret = mcpwm_timer_enable(handle->timer);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to enable timer: %s", esp_err_to_name(ret));
    if (handle->fault) {
      mcpwm_del_fault(handle->fault);
    }
    mcpwm_del_generator(handle->gen_b);
    mcpwm_del_generator(handle->gen_a);
    mcpwm_del_comparator(handle->comparator);
    mcpwm_del_operator(handle->operator);
    mcpwm_del_timer(handle->timer);
    return ret;
  }

  ret = mcpwm_timer_start_stop(handle->timer, MCPWM_TIMER_START_NO_STOP);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to start timer: %s", esp_err_to_name(ret));
    mcpwm_timer_disable(handle->timer);
    if (handle->fault) {
      mcpwm_del_fault(handle->fault);
    }
    mcpwm_del_generator(handle->gen_b);
    mcpwm_del_generator(handle->gen_a);
    mcpwm_del_comparator(handle->comparator);
    mcpwm_del_operator(handle->operator);
    mcpwm_del_timer(handle->timer);
    return ret;
  }

  /* Store configuration */
  handle->group_id     = config->group_id;
  handle->pwm_freq_hz  = config->pwm_freq_hz;
  handle->period_ticks = period_ticks;
  handle->current_duty = (float)k_motor_duty_zero;
  handle->initialized  = true;

  /* Set initial duty to 0 (stopped) */
  star_motor_set_duty(handle, (float)k_motor_duty_zero);

  ESP_LOGI(s_tag,
           "Motor initialized: group=%d, freq=%luHz, pins=A%d/B%d, dead_time=%luns",
           config->group_id,
           config->pwm_freq_hz,
           config->pin_pwm_a,
           config->pin_pwm_b,
           config->dead_time_ns);

  return ESP_OK;
}

esp_err_t star_motor_deinit(star_motor_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Stop motor first */
  star_motor_stop(handle, false);

  /* Stop and disable timer */
  mcpwm_timer_start_stop(handle->timer, MCPWM_TIMER_STOP_EMPTY);
  mcpwm_timer_disable(handle->timer);

  /* Delete all MCPWM resources */
  if (handle->fault) {
    mcpwm_del_fault(handle->fault);
  }
  mcpwm_del_generator(handle->gen_b);
  mcpwm_del_generator(handle->gen_a);
  mcpwm_del_comparator(handle->comparator);
  mcpwm_del_operator(handle->operator);
  mcpwm_del_timer(handle->timer);

  /* Clear handle */
  memset(handle, 0, sizeof(star_motor_handle_t));

  ESP_LOGI(s_tag, "Motor deinitialized");
  return ESP_OK;
}

esp_err_t star_motor_set_duty(star_motor_handle_t* handle, float duty)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Clamp duty cycle */
  duty = internal_clamp_duty(duty);

  /* Calculate compare value (duty is -100 to +100, so we use absolute value) */
  float    abs_duty = fabsf(duty);
  uint32_t compare_ticks =
    (uint32_t)((abs_duty / (float)k_motor_percent_scale) * handle->period_ticks);

  /* Set comparator value */
  esp_err_t ret = mcpwm_comparator_set_compare_value(handle->comparator, compare_ticks);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to set compare value");

  /* Configure direction (swap generator actions based on duty sign) */
  if (duty >= (float)k_motor_duty_zero) {
    /* Forward: A = PWM, B = LOW */
    mcpwm_generator_set_action_on_compare_event(
      handle->gen_a,
      MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                     handle->comparator,
                                     MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_compare_event(
      handle->gen_a,
      MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                     handle->comparator,
                                     MCPWM_GEN_ACTION_LOW));
    mcpwm_generator_set_actions_on_timer_event(
      handle->gen_b,
      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                   MCPWM_TIMER_EVENT_EMPTY,
                                   MCPWM_GEN_ACTION_LOW),
      MCPWM_GEN_TIMER_EVENT_ACTION_END());
  } else {
    /* Reverse: A = LOW, B = PWM */
    mcpwm_generator_set_actions_on_timer_event(
      handle->gen_a,
      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                   MCPWM_TIMER_EVENT_EMPTY,
                                   MCPWM_GEN_ACTION_LOW),
      MCPWM_GEN_TIMER_EVENT_ACTION_END());
    mcpwm_generator_set_action_on_compare_event(
      handle->gen_b,
      MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                     handle->comparator,
                                     MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_compare_event(
      handle->gen_b,
      MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                     handle->comparator,
                                     MCPWM_GEN_ACTION_LOW));
  }

  handle->current_duty = duty;

  ESP_LOGD(s_tag, "Motor duty set: %.1f%% (compare=%lu ticks)", duty, compare_ticks);
  return ESP_OK;
}

esp_err_t star_motor_stop(star_motor_handle_t* handle, bool brake)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  if (brake) {
    /* Brake: both outputs high (short circuit brake) */
    mcpwm_generator_set_force_level(handle->gen_a, 1, true);
    mcpwm_generator_set_force_level(handle->gen_b, 1, true);
  } else {
    /* Coast: both outputs low */
    mcpwm_generator_set_force_level(handle->gen_a, 0, true);
    mcpwm_generator_set_force_level(handle->gen_b, 0, true);
  }

  handle->current_duty = (float)k_motor_duty_zero;

  ESP_LOGD(s_tag, "Motor stopped (%s)", brake ? "brake" : "coast");
  return ESP_OK;
}

esp_err_t star_motor_get_duty(const star_motor_handle_t* handle, float* out_duty)
{
  ESP_RETURN_ON_FALSE(handle && out_duty, ESP_ERR_INVALID_ARG, s_tag, "Handle or out_duty is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  *out_duty = handle->current_duty;
  return ESP_OK;
}

/* --- Private Functions --- */

static inline float internal_clamp_duty(float duty)
{
  if (duty < (float)k_motor_duty_min) {
    return (float)k_motor_duty_min;
  }
  if (duty > (float)k_motor_duty_max) {
    return (float)k_motor_duty_max;
  }
  return duty;
}

static esp_err_t internal_configure_generators(star_motor_handle_t* handle, bool invert_pwm)
{
  /* Configure generator A actions (forward PWM when duty > 0) */
  esp_err_t ret;

  ret =
    mcpwm_generator_set_action_on_timer_event(handle->gen_a,
                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                                           MCPWM_TIMER_EVENT_EMPTY,
                                                                           MCPWM_GEN_ACTION_LOW));
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to set gen_a timer action");

  ret = mcpwm_generator_set_action_on_compare_event(
    handle->gen_a,
    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                   handle->comparator,
                                   MCPWM_GEN_ACTION_HIGH));
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to set gen_a compare up action");

  ret = mcpwm_generator_set_action_on_compare_event(
    handle->gen_a,
    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                   handle->comparator,
                                   MCPWM_GEN_ACTION_LOW));
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to set gen_a compare down action");

  /* Configure generator B actions (initially low, will be set during duty update) */
  ret =
    mcpwm_generator_set_action_on_timer_event(handle->gen_b,
                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                                           MCPWM_TIMER_EVENT_EMPTY,
                                                                           MCPWM_GEN_ACTION_LOW));
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to set gen_b timer action");

  return ESP_OK;
}
