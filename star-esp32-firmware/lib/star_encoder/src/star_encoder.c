/* lib/star_encoder/src/star_encoder.c */

#include "star_encoder.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>

/* --- Constants --- */

static const char* s_TAG = "STAR_ENCODER";

/* --- Private Function Prototypes --- */

static bool internal_encoder_on_reach_callback(pcnt_unit_handle_t    unit,
                                                const pcnt_watch_event_data_t* event_data,
                                                void*                 user_ctx);

/* --- Public Functions --- */

esp_err_t star_encoder_init(star_encoder_handle_t* handle, const star_encoder_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Already initialized");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_encoder_handle_t));

  esp_err_t ret = ESP_OK;

  /* Configure PCNT unit */
  pcnt_unit_config_t unit_config = {
    .high_limit = config->high_limit,
    .low_limit  = config->low_limit,
    .flags.accum_count = 0, /* Clear counter on reaching limits */
  };

  ret = pcnt_new_unit(&unit_config, &handle->unit_handle);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to create PCNT unit");

  /* Configure glitch filter */
  if (config->filter_value > 0) {
    pcnt_glitch_filter_config_t filter_config = {
      .max_glitch_ns = config->filter_value * 12.5f, /* APB clock = 80MHz = 12.5ns */
    };
    ret = pcnt_unit_set_glitch_filter(handle->unit_handle, &filter_config);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to set glitch filter: %s", esp_err_to_name(ret));
      pcnt_del_unit(handle->unit_handle);
      return ret;
    }
  }

  /* Configure channel A (encoder phase A) */
  pcnt_chan_config_t chan_a_config = {
    .edge_gpio_num = config->pin_a,
    .level_gpio_num = config->pin_b,
  };
  ret = pcnt_new_channel(handle->unit_handle, &chan_a_config, &handle->channel_a);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to create channel A: %s", esp_err_to_name(ret));
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Configure channel B (encoder phase B) */
  pcnt_chan_config_t chan_b_config = {
    .edge_gpio_num = config->pin_b,
    .level_gpio_num = config->pin_a,
  };
  ret = pcnt_new_channel(handle->unit_handle, &chan_b_config, &handle->channel_b);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to create channel B: %s", esp_err_to_name(ret));
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
    ESP_LOGE(s_TAG, "Failed to register event callbacks: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Add watch points for overflow/underflow */
  ret = pcnt_unit_add_watch_point(handle->unit_handle, config->high_limit);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to add high limit watch point: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  ret = pcnt_unit_add_watch_point(handle->unit_handle, config->low_limit);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to add low limit watch point: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Enable PCNT unit */
  ret = pcnt_unit_enable(handle->unit_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to enable PCNT unit: %s", esp_err_to_name(ret));
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Start counting */
  ret = pcnt_unit_start(handle->unit_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to start PCNT unit: %s", esp_err_to_name(ret));
    pcnt_unit_disable(handle->unit_handle);
    pcnt_del_channel(handle->channel_b);
    pcnt_del_channel(handle->channel_a);
    pcnt_del_unit(handle->unit_handle);
    return ret;
  }

  /* Store configuration */
  handle->pcnt_unit      = config->pcnt_unit;
  handle->pin_a          = config->pin_a;
  handle->pin_b          = config->pin_b;
  handle->last_count     = 0;
  handle->last_time_us   = esp_timer_get_time();
  handle->overflow_count = 0;
  handle->initialized    = true;

  ESP_LOGI(s_TAG,
           "Encoder initialized: unit=%d, pins=A%d/B%d, limits=[%d,%d]",
           config->pcnt_unit,
           config->pin_a,
           config->pin_b,
           config->low_limit,
           config->high_limit);

  return ESP_OK;
}

esp_err_t star_encoder_deinit(star_encoder_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Stop and disable unit */
  pcnt_unit_stop(handle->unit_handle);
  pcnt_unit_disable(handle->unit_handle);

  /* Delete channels and unit */
  pcnt_del_channel(handle->channel_b);
  pcnt_del_channel(handle->channel_a);
  pcnt_del_unit(handle->unit_handle);

  /* Clear handle */
  memset(handle, 0, sizeof(star_encoder_handle_t));

  ESP_LOGI(s_TAG, "Encoder deinitialized");
  return ESP_OK;
}

esp_err_t star_encoder_get_count(star_encoder_handle_t* handle, int32_t* out_count)
{
  ESP_RETURN_ON_FALSE(handle && out_count, ESP_ERR_INVALID_ARG, s_TAG, "Handle or out_count is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Read hardware counter */
  int hw_count;
  esp_err_t ret = pcnt_unit_get_count(handle->unit_handle, &hw_count);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to get count");

  /* Combine hardware count with overflow tracking */
  *out_count = hw_count + (handle->overflow_count * 0x10000); /* 16-bit overflow */

  return ESP_OK;
}

esp_err_t star_encoder_reset_count(star_encoder_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Clear hardware counter */
  esp_err_t ret = pcnt_unit_clear_count(handle->unit_handle);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to clear count");

  /* Reset overflow tracking */
  handle->overflow_count = 0;
  handle->last_count     = 0;
  handle->last_time_us   = esp_timer_get_time();

  ESP_LOGD(s_TAG, "Encoder count reset");
  return ESP_OK;
}

esp_err_t star_encoder_get_velocity_rpm(star_encoder_handle_t* handle,
                                         float                  dt_ms,
                                         uint32_t               counts_per_rev,
                                         float*                 out_velocity_rpm)
{
  ESP_RETURN_ON_FALSE(handle && out_velocity_rpm,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Handle or out_velocity_rpm is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");
  ESP_RETURN_ON_FALSE(counts_per_rev > 0, ESP_ERR_INVALID_ARG, s_TAG, "counts_per_rev must be > 0");
  ESP_RETURN_ON_FALSE(dt_ms > 0.0f, ESP_ERR_INVALID_ARG, s_TAG, "dt_ms must be > 0");

  /* Get current count */
  int32_t current_count;
  esp_err_t ret = star_encoder_get_count(handle, &current_count);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to get count");

  /* Calculate count delta */
  int32_t count_delta = current_count - handle->last_count;

  /* Calculate velocity: (counts/dt_ms) * (60000 ms/min) / (counts/rev) */
  *out_velocity_rpm = (count_delta * 60000.0f) / (dt_ms * counts_per_rev);

  /* Update last values */
  handle->last_count   = current_count;
  handle->last_time_us = esp_timer_get_time();

  return ESP_OK;
}

/* --- Private Functions --- */

static bool internal_encoder_on_reach_callback(pcnt_unit_handle_t             unit,
                                                const pcnt_watch_event_data_t* event_data,
                                                void*                          user_ctx)
{
  star_encoder_handle_t* handle = (star_encoder_handle_t*)user_ctx;
  if (!handle) {
    return false;
  }

  /* Track overflow/underflow events */
  if (event_data->watch_point_value > 0) {
    handle->overflow_count++; /* High limit reached - overflow */
  } else {
    handle->overflow_count--; /* Low limit reached - underflow */
  }

  return false; /* Don't yield from ISR */
}
