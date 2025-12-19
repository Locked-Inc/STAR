/* lib/star_encoder/src/star_encoder.c */

#include "star_encoder.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

/* --- Constants --- */

/**
 * @brief Logging tag for ESP-IDF logging macros
 */
static const char* s_tag = "STAR_ENCODER";

/**
 * @brief APB clock period in nanoseconds
 *
 * The ESP32 APB clock runs at 80 MHz, giving a period of 12.5 ns.
 * Used to convert glitch filter values from APB clock cycles to nanoseconds
 * as required by pcnt_glitch_filter_config_t.
 */
static const float s_apb_clock_period_ns = 12.5f;

/**
 * @brief Conversion factor from milliseconds to minutes
 *
 * Used in velocity calculation to convert counts/ms to RPM.
 * 60000 ms = 1 minute
 */
static const float s_ms_per_minute = 60000.0f;

/* --- Private Function Prototypes --- */

static bool internal_encoder_on_reach_callback(pcnt_unit_handle_t             unit,
                                               const pcnt_watch_event_data_t* event_data,
                                               void*                          user_ctx);

/* --- Public Functions --- */

esp_err_t star_encoder_init(star_encoder_handle_t* handle, const star_encoder_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Already initialized");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_encoder_handle_t));

  esp_err_t ret = ESP_OK;

  /* Configure PCNT unit */
  pcnt_unit_config_t unit_config = {
    .high_limit        = config->high_limit,
    .low_limit         = config->low_limit,
    .flags.accum_count = 0, /* Clear counter on reaching limits */
  };

  ret = pcnt_new_unit(&unit_config, &handle->unit_handle);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to create PCNT unit");

  /* Configure glitch filter */
  if (config->filter_value > 0) {
    pcnt_glitch_filter_config_t filter_config = {
      .max_glitch_ns = (uint32_t)(config->filter_value * s_apb_clock_period_ns),
    };
    ret = pcnt_unit_set_glitch_filter(handle->unit_handle, &filter_config);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag, "Failed to set glitch filter: %s", esp_err_to_name(ret));
      pcnt_del_unit(handle->unit_handle);
      return ret;
    }
  }

  /* Configure channel A (encoder phase A) */
  pcnt_chan_config_t chan_a_config = {
    .edge_gpio_num  = config->pin_a,
    .level_gpio_num = config->pin_b,
  };
  ret = pcnt_new_channel(handle->unit_handle, &chan_a_config, &handle->channel_a);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create channel A: %s", esp_err_to_name(ret));
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Configure channel B (encoder phase B) */
  pcnt_chan_config_t chan_b_config = {
    .edge_gpio_num  = config->pin_b,
    .level_gpio_num = config->pin_a,
  };
  ret = pcnt_new_channel(handle->unit_handle, &chan_b_config, &handle->channel_b);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to create channel B: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Set edge and level actions for quadrature decoding */
  /* Channel A: increment on positive edge when B=0, decrement when B=1 */
  pcnt_channel_set_edge_action(handle->channel_a,
                               PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                               PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_channel_set_level_action(handle->channel_a,
                                PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

  /* Channel B: increment on positive edge when A=1, decrement when A=0 */
  pcnt_channel_set_edge_action(handle->channel_b,
                               PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                               PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_channel_set_level_action(handle->channel_b,
                                PCNT_CHANNEL_LEVEL_ACTION_INVERSE,
                                PCNT_CHANNEL_LEVEL_ACTION_KEEP);

  /* Register overflow/underflow watch events */
  pcnt_event_callbacks_t cbs = {
    .on_reach = internal_encoder_on_reach_callback,
  };
  ret = pcnt_unit_register_event_callbacks(handle->unit_handle, &cbs, handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to register event callbacks: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Add watch points for overflow/underflow */
  ret = pcnt_unit_add_watch_point(handle->unit_handle, config->high_limit);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add high limit watch point: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  ret = pcnt_unit_add_watch_point(handle->unit_handle, config->low_limit);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to add low limit watch point: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Enable PCNT unit */
  ret = pcnt_unit_enable(handle->unit_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to enable PCNT unit: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Start counting */
  ret = pcnt_unit_start(handle->unit_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to start PCNT unit: %s", esp_err_to_name(ret));
    pcnt_unit_disable(handle->unit_handle);
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Store configuration */
  handle->pin_a          = config->pin_a;
  handle->pin_b          = config->pin_b;
  handle->high_limit     = config->high_limit;
  handle->low_limit      = config->low_limit;
  handle->last_count     = 0;
  handle->last_time_us   = esp_timer_get_time();
  handle->overflow_count = 0;
  handle->initialized    = true;

  ESP_LOGI(s_tag,
           "Encoder initialized: pins=A%d/B%d, limits=[%d,%d]",
           config->pin_a,
           config->pin_b,
           config->low_limit,
           config->high_limit);

  return ESP_OK;
}

esp_err_t star_encoder_deinit(star_encoder_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Stop and disable unit */
  pcnt_unit_stop(handle->unit_handle);
  pcnt_unit_disable(handle->unit_handle);

  /* Delete channels and unit */
  pcnt_del_channel(handle->channel_b);
  pcnt_del_channel(handle->channel_a);
  pcnt_del_unit(handle->unit_handle);

  /* Clear handle */
  memset(handle, 0, sizeof(star_encoder_handle_t));

  ESP_LOGI(s_tag, "Encoder deinitialized");
  return ESP_OK;
}

esp_err_t star_encoder_get_count(star_encoder_handle_t* handle, int32_t* out_count)
{
  ESP_RETURN_ON_FALSE(handle && out_count,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Handle or out_count is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /*
   * Read hardware counter
   * Note: pcnt_unit_get_count() requires int* per ESP-IDF API.
   * We explicitly cast to int32_t when combining with overflow tracking.
   */
  int       hw_count = 0;
  esp_err_t ret      = pcnt_unit_get_count(handle->unit_handle, &hw_count);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to get count");

  /* Combine hardware count with overflow tracking */
  int32_t range = (int32_t)(handle->high_limit - handle->low_limit);
  *out_count    = (int32_t)hw_count + (handle->overflow_count * range);

  return ESP_OK;
}

esp_err_t star_encoder_reset_count(star_encoder_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Clear hardware counter */
  esp_err_t ret = pcnt_unit_clear_count(handle->unit_handle);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to clear count");

  /* Reset overflow tracking */
  handle->overflow_count = 0;
  handle->last_count     = 0;
  handle->last_time_us   = esp_timer_get_time();

  ESP_LOGD(s_tag, "Encoder count reset");
  return ESP_OK;
}

esp_err_t star_encoder_get_velocity_rpm(star_encoder_handle_t* handle,
                                        float                  dt_ms,
                                        uint32_t               counts_per_rev,
                                        float*                 out_velocity_rpm)
{
  ESP_RETURN_ON_FALSE(handle && out_velocity_rpm,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Handle or out_velocity_rpm is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");
  ESP_RETURN_ON_FALSE(counts_per_rev > 0, ESP_ERR_INVALID_ARG, s_tag, "counts_per_rev must be > 0");
  ESP_RETURN_ON_FALSE(dt_ms > 0.0f, ESP_ERR_INVALID_ARG, s_tag, "dt_ms must be > 0");

  /* Get current count */
  int32_t   current_count;
  esp_err_t ret = star_encoder_get_count(handle, &current_count);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to get count");

  /* Calculate count delta */
  int32_t count_delta = current_count - handle->last_count;

  /* Calculate velocity: (counts/dt_ms) * (ms/min) / (counts/rev) = rev/min */
  *out_velocity_rpm = ((float)count_delta * s_ms_per_minute) / (dt_ms * (float)counts_per_rev);

  /* Update last values */
  handle->last_count   = current_count;
  handle->last_time_us = esp_timer_get_time();

  return ESP_OK;
}

/* --- Private Functions --- */

/**
 * @brief PCNT watch point callback for overflow/underflow tracking
 *
 * This ISR callback is invoked when the PCNT counter reaches the configured
 * high or low limit watch points. It updates the overflow counter to extend
 * the effective counting range beyond the 16-bit hardware limit.
 *
 * Hardware behavior:
 * - Counter wraps around when reaching limits (not accumulated)
 * - High limit reached: indicates positive direction overflow
 * - Low limit reached: indicates negative direction underflow
 *
 * @param[in] unit       PCNT unit handle (unused, required by ESP-IDF callback signature)
 * @param[in] event_data Watch event data containing the watch point value
 * @param[in] user_ctx   User context pointer (star_encoder_handle_t*)
 * @return false Always returns false to not yield from ISR
 */
static bool internal_encoder_on_reach_callback(pcnt_unit_handle_t             unit,
                                               const pcnt_watch_event_data_t* event_data,
                                               void*                          user_ctx)
{
  (void)unit; /* Unused parameter - required by ESP-IDF callback signature */

  star_encoder_handle_t* handle = (star_encoder_handle_t*)user_ctx;
  if (!handle) {
    return false;
  }

  /* Track overflow/underflow events */
  if (event_data->watch_point_value == handle->high_limit) {
    handle->overflow_count++; /* High limit reached - overflow */
  } else if (event_data->watch_point_value == handle->low_limit) {
    handle->overflow_count--; /* Low limit reached - underflow */
  }

  return false; /* Don't yield from ISR */
}
