/**
 * @file star_encoder_mux.c
 * @brief Multiplexed quadrature encoder driver implementation
 * @details
 * Implements support for reading 4 quadrature encoders through a 74HC4052 multiplexer.
 * The driver sequentially switches between encoders and reads position/velocity data.
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "star_encoder_mux.h"

#include "driver/gpio.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

/* --- Constants --- */

/**
 * @brief Logging tag for ESP-IDF logging macros
 */
static const char* s_tag = "STAR_ENCODER_MUX";

/**
 * @brief Conversion factor from milliseconds to minutes
 *
 * Used in velocity calculation to convert counts/ms to RPM.
 * 60000 ms = 1 minute
 */
static const float s_ms_per_minute = 60000.0f;

/* --- Private Function Prototypes --- */

static esp_err_t internal_select_encoder(star_encoder_mux_handle_t* handle, uint8_t encoder_idx);
static esp_err_t internal_read_encoder_count(star_encoder_mux_handle_t* handle,
                                             uint8_t                    encoder_idx,
                                             int32_t*                   out_position,
                                             float*                     out_velocity_rpm,
                                             float                      dt_ms,
                                             uint32_t                   counts_per_rev);

/* --- Public Functions --- */

esp_err_t star_encoder_mux_init(star_encoder_mux_handle_t*       handle,
                                const star_encoder_mux_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Already initialized");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_encoder_mux_handle_t));

  esp_err_t ret = ESP_OK;

  /* Configure multiplexer select lines as outputs */
  gpio_config_t gpio_conf = {
    .pin_bit_mask = (1ULL << config->pin_sel0) | (1ULL << config->pin_sel1),
    .mode         = GPIO_MODE_OUTPUT,
    .pull_up_en   = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
  };

  ret = gpio_config(&gpio_conf);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to configure select line GPIOs");

  /* Set select lines to encoder 0 initially */
  gpio_set_level(config->pin_sel0, 0);
  gpio_set_level(config->pin_sel1, 0);

  /* Initialize shared encoder instance */
  star_encoder_config_t enc_config = {
    .pin_a        = config->pin_out_a,
    .pin_b        = config->pin_out_b,
    .filter_value = config->filter_value,
    .high_limit   = config->high_limit,
    .low_limit    = config->low_limit,
  };

  ret = star_encoder_init(&handle->encoder, &enc_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to initialize encoder: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Store configuration */
  handle->pin_sel0         = config->pin_sel0;
  handle->pin_sel1         = config->pin_sel1;
  handle->settling_time_us = config->settling_time_us;
  handle->last_time_us     = esp_timer_get_time();

  /* Initialize position tracking */
  for (int i = 0; i < k_encoder_count; i++) {
    handle->positions[i]   = 0;
    handle->last_counts[i] = 0;
  }

  handle->initialized = true;

  ESP_LOGI(s_tag,
           "Encoder mux initialized: sel=%d/%d, out=%d/%d, settling=%luμs",
           config->pin_sel0,
           config->pin_sel1,
           config->pin_out_a,
           config->pin_out_b,
           config->settling_time_us);

  return ESP_OK;
}

esp_err_t star_encoder_mux_deinit(star_encoder_mux_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Deinitialize shared encoder */
  star_encoder_deinit(&handle->encoder);

  /* Reset select lines to low */
  gpio_set_level(handle->pin_sel0, 0);
  gpio_set_level(handle->pin_sel1, 0);

  /* Clear handle */
  memset(handle, 0, sizeof(star_encoder_mux_handle_t));

  ESP_LOGI(s_tag, "Encoder mux deinitialized");
  return ESP_OK;
}

esp_err_t star_encoder_mux_read_all(star_encoder_mux_handle_t* handle,
                                    int32_t                    out_positions[k_encoder_count],
                                    float                      out_velocities_rpm[k_encoder_count],
                                    float                      dt_ms,
                                    uint32_t                   counts_per_rev)
{
  ESP_RETURN_ON_FALSE(handle && out_positions && out_velocities_rpm,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Handle or output arrays are NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");
  ESP_RETURN_ON_FALSE(dt_ms > 0.0f, ESP_ERR_INVALID_ARG, s_tag, "dt_ms must be > 0");
  ESP_RETURN_ON_FALSE(counts_per_rev > 0, ESP_ERR_INVALID_ARG, s_tag, "counts_per_rev must be > 0");

  esp_err_t ret = ESP_OK;

  /* Sequentially read all 4 encoders */
  for (uint8_t i = 0; i < k_encoder_count; i++) {
    ret = internal_read_encoder_count(handle,
                                      i,
                                      &out_positions[i],
                                      &out_velocities_rpm[i],
                                      dt_ms,
                                      counts_per_rev);

    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to read encoder %d: %s", i, esp_err_to_name(ret));
      /* Continue reading other encoders even if one fails */
      out_positions[i]      = handle->positions[i]; /* Use last known position */
      out_velocities_rpm[i] = 0.0f;                 /* Zero velocity on error */
    }
  }

  /* Update timestamp */
  handle->last_time_us = esp_timer_get_time();

  return ESP_OK;
}

esp_err_t star_encoder_mux_reset(star_encoder_mux_handle_t* handle, uint8_t encoder_idx)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");
  ESP_RETURN_ON_FALSE(encoder_idx < k_encoder_count,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "encoder_idx must be < %d",
                      k_encoder_count);

  /* Reset position tracking for specified encoder */
  handle->positions[encoder_idx]   = 0;
  handle->last_counts[encoder_idx] = 0;

  ESP_LOGD(s_tag, "Encoder %d position reset", encoder_idx);
  return ESP_OK;
}

esp_err_t star_encoder_mux_reset_all(star_encoder_mux_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Reset position tracking for all encoders */
  for (int i = 0; i < k_encoder_count; i++) {
    handle->positions[i]   = 0;
    handle->last_counts[i] = 0;
  }

  /* Reset PCNT counter */
  star_encoder_reset_count(&handle->encoder);

  ESP_LOGD(s_tag, "All encoder positions reset");
  return ESP_OK;
}

/* --- Private Functions --- */

/**
 * @brief Select a specific encoder via multiplexer
 *
 * Sets the multiplexer select lines to route the specified encoder's
 * signals to the shared output pins.
 *
 * Select line truth table for 74HC4052:
 *   SEL1 | SEL0 | Selected Encoder
 *   -----|------|------------------
 *     0  |   0  |        0
 *     0  |   1  |        1
 *     1  |   0  |        2
 *     1  |   1  |        3
 *
 * @param[in] handle      Pointer to encoder mux handle
 * @param[in] encoder_idx Encoder index (0-3)
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid encoder index
 */
static esp_err_t internal_select_encoder(star_encoder_mux_handle_t* handle, uint8_t encoder_idx)
{
  if (encoder_idx >= k_encoder_count) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Set select lines based on encoder index (binary encoding) */
  gpio_set_level(handle->pin_sel0, encoder_idx & 0x01);        /* Bit 0 */
  gpio_set_level(handle->pin_sel1, (encoder_idx >> 1) & 0x01); /* Bit 1 */

  /* Wait for multiplexer settling time */
  ets_delay_us(handle->settling_time_us);

  return ESP_OK;
}

/**
 * @brief Read position and velocity from a specific encoder
 *
 * Switches to the specified encoder, reads the PCNT count, updates the
 * accumulated position, and calculates velocity.
 *
 * @param[in]  handle           Pointer to encoder mux handle
 * @param[in]  encoder_idx      Encoder index (0-3)
 * @param[out] out_position     Pointer to store absolute position
 * @param[out] out_velocity_rpm Pointer to store velocity (RPM)
 * @param[in]  dt_ms            Time since last call (ms)
 * @param[in]  counts_per_rev   Encoder counts per revolution
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid parameters
 *   - Other ESP-IDF error codes from encoder read
 */
static esp_err_t internal_read_encoder_count(star_encoder_mux_handle_t* handle,
                                             uint8_t                    encoder_idx,
                                             int32_t*                   out_position,
                                             float*                     out_velocity_rpm,
                                             float                      dt_ms,
                                             uint32_t                   counts_per_rev)
{
  /* Select encoder via multiplexer */
  esp_err_t ret = internal_select_encoder(handle, encoder_idx);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to select encoder %d", encoder_idx);

  /* Read current PCNT count */
  int32_t current_count;
  ret = star_encoder_get_count(&handle->encoder, &current_count);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to read encoder %d count", encoder_idx);

  /* Calculate delta count from last read */
  int32_t delta_count = current_count - handle->last_counts[encoder_idx];

  /* Update accumulated position */
  handle->positions[encoder_idx] += delta_count;
  *out_position = handle->positions[encoder_idx];

  /* Calculate velocity: (counts/dt_ms) * (ms/min) / (counts/rev) = rev/min */
  *out_velocity_rpm = ((float)delta_count * s_ms_per_minute) / (dt_ms * (float)counts_per_rev);

  /* Store current count for next delta calculation */
  handle->last_counts[encoder_idx] = current_count;

  return ESP_OK;
}
