/* tests/mocks/mock_rx_obstacle_detect.c */

/**
 * @file mock_rx_obstacle_detect.c
 * @brief Mock Obstacle Detection Implementation
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_obstacle_detect.h"

#include <string.h>

/* Static return values */
static rx_err_t s_init_return  = k_rx_ok;
static rx_err_t s_start_return = k_rx_ok;
static rx_err_t s_stop_return  = k_rx_ok;

/* Static call counts */
static uint32_t s_init_count  = 0;
static uint32_t s_start_count = 0;
static uint32_t s_stop_count  = 0;

/* Statistics */
static uint32_t s_stats_polls     = 0;
static uint32_t s_stats_events    = 0;
static uint32_t s_stats_false_pos = 0;

/* Stored handle for callback */
static rx_obstacle_detect_t* s_current_handle = nullptr;

void mock_obstacle_detect_reset(void)
{
  s_init_return     = k_rx_ok;
  s_start_return    = k_rx_ok;
  s_stop_return     = k_rx_ok;
  s_init_count      = 0;
  s_start_count     = 0;
  s_stop_count      = 0;
  s_stats_polls     = 0;
  s_stats_events    = 0;
  s_stats_false_pos = 0;
  s_current_handle  = nullptr;
}

void mock_obstacle_detect_set_init_return(rx_err_t err)
{
  s_init_return = err;
}
void mock_obstacle_detect_set_start_return(rx_err_t err)
{
  s_start_return = err;
}
void mock_obstacle_detect_set_stop_return(rx_err_t err)
{
  s_stop_return = err;
}

void mock_obstacle_detect_trigger_callback(bool detected, uint8_t sensor, float distance)
{
  if (s_current_handle != nullptr && s_current_handle->callback != nullptr) {
    s_current_handle->callback(detected, sensor, distance, s_current_handle->user_data);
  }
}

void mock_obstacle_detect_set_stats(uint32_t polls, uint32_t events, uint32_t false_pos)
{
  s_stats_polls     = polls;
  s_stats_events    = events;
  s_stats_false_pos = false_pos;
}

uint32_t mock_obstacle_detect_get_init_count(void)
{
  return s_init_count;
}
uint32_t mock_obstacle_detect_get_start_count(void)
{
  return s_start_count;
}
uint32_t mock_obstacle_detect_get_stop_count(void)
{
  return s_stop_count;
}
bool mock_obstacle_detect_was_initialized(void)
{
  return s_init_count > 0;
}

rx_err_t rx_obstacle_detect_init(rx_obstacle_detect_t*              handle,
                                 const rx_obstacle_detect_config_t* config)
{
  s_init_count++;

  if (handle == nullptr || config == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_init_return == k_rx_ok) {
    handle->sensor_count           = config->sensor_count;
    handle->detection_threshold_cm = config->detection_threshold_cm;
    handle->debounce_samples       = config->debounce_samples;
    handle->poll_interval_ms       = config->poll_interval_ms;
    handle->callback               = config->callback;
    handle->user_data              = config->user_data;
    handle->initialized            = true;
    handle->running                = false;
    s_current_handle               = handle;
  }

  return s_init_return;
}

rx_err_t rx_obstacle_detect_deinit(rx_obstacle_detect_t* handle)
{
  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  (void)memset(handle, 0, sizeof(*handle));
  s_current_handle = nullptr;

  return k_rx_ok;
}

rx_err_t rx_obstacle_detect_start(rx_obstacle_detect_t* handle)
{
  s_start_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_start_return == k_rx_ok) {
    handle->running = true;
  }

  return s_start_return;
}

rx_err_t rx_obstacle_detect_stop(rx_obstacle_detect_t* handle)
{
  s_stop_count++;

  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_stop_return == k_rx_ok) {
    handle->running = false;
  }

  return s_stop_return;
}

rx_err_t rx_obstacle_detect_get_stats(rx_obstacle_detect_t* handle,
                                      uint32_t*             total_polls,
                                      uint32_t*             obstacle_events,
                                      uint32_t*             false_positives)
{
  if (handle == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (total_polls != nullptr) {
    *total_polls = s_stats_polls;
  }
  if (obstacle_events != nullptr) {
    *obstacle_events = s_stats_events;
  }
  if (false_positives != nullptr) {
    *false_positives = s_stats_false_pos;
  }

  return k_rx_ok;
}
