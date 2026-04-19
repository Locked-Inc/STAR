/**
 * @file mock_rx_comm_manager.c
 * @brief Mock Communication Manager Implementation
 *
 * @details
 * Implements mock rx_comm_manager functions for unit testing.
 * Tracks calls, stores parameters, and allows configurable return values.
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rx_comm_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* =============================================================================
 * Static Variables - Return Values
 * =============================================================================
 */

/** @brief Return value for rx_comm_manager_init() */
static rx_err_t s_init_return = k_rx_ok;

/** @brief Return value for rx_comm_manager_deinit() */
static rx_err_t s_deinit_return = k_rx_ok;

/** @brief Return value for rx_comm_manager_poll() */
static rx_err_t s_poll_return = k_rx_err_timeout;

/** @brief Return value for rx_comm_manager_send() */
static rx_err_t s_send_return = k_rx_ok;

/** @brief Return value for rx_comm_manager_respond() */
static rx_err_t s_respond_return = k_rx_ok;

/* =============================================================================
 * Static Variables - Call Counts
 * =============================================================================
 */

/** @brief Call count for rx_comm_manager_init() */
static uint32_t s_init_count = 0;

/** @brief Call count for rx_comm_manager_deinit() */
static uint32_t s_deinit_count = 0;

/** @brief Call count for rx_comm_manager_poll() */
static uint32_t s_poll_count = 0;

/** @brief Call count for rx_comm_manager_send() */
static uint32_t s_send_count = 0;

/** @brief Call count for rx_comm_manager_respond() */
static uint32_t s_respond_count = 0;

/* =============================================================================
 * Static Variables - Frame Queue (for simulating incoming frames)
 * =============================================================================
 */

/**
 * @struct mock_queued_frame_t
 * @brief Queued frame for poll delivery
 */
typedef struct {
  rx_frame_t        frame;   /**< Frame data */
  rx_comm_channel_t channel; /**< Channel */
  bool              valid;   /**< Valid entry */
} mock_queued_frame_t;

/** @brief Frame queue */
static mock_queued_frame_t s_frame_queue[k_mock_comm_frame_queue_size];

/** @brief Queue read index */
static uint32_t s_queue_read_idx = 0;

/** @brief Queue write index */
static uint32_t s_queue_write_idx = 0;

/** @brief Queue count */
static uint32_t s_queue_count = 0;

/* =============================================================================
 * Static Variables - Send Tracking
 * =============================================================================
 */

/** @brief Last channel sent to */
static rx_comm_channel_t s_last_send_channel = k_comm_channel_uart;

/** @brief Last frame type sent */
static rx_frame_type_t s_last_send_type = k_frame_type_ping;

/** @brief Last payload sent */
static uint8_t s_last_send_payload[k_mock_comm_max_payload_size];

/** @brief Last payload length sent */
static uint32_t s_last_send_payload_len = 0;

/* =============================================================================
 * Static Variables - Channel Ready Status
 * =============================================================================
 */

/** @brief Channel ready status */
static bool s_channel_ready[k_comm_channel_count] = {true, true, true};

/* =============================================================================
 * Static Variables - Manager Pointer (for callback invocation)
 * =============================================================================
 */

/** @brief Stored manager pointer for callback invocation */
static rx_comm_manager_t* s_current_manager = nullptr;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_comm_manager_reset(void)
{
  /* Reset return values to defaults */
  s_init_return    = k_rx_ok;
  s_deinit_return  = k_rx_ok;
  s_poll_return    = k_rx_err_timeout;
  s_send_return    = k_rx_ok;
  s_respond_return = k_rx_ok;

  /* Reset call counts */
  s_init_count    = 0;
  s_deinit_count  = 0;
  s_poll_count    = 0;
  s_send_count    = 0;
  s_respond_count = 0;

  /* Reset frame queue */
  for (uint32_t i = 0; i < k_mock_comm_frame_queue_size; i++) {
    s_frame_queue[i].valid = false;
  }
  s_queue_read_idx  = 0;
  s_queue_write_idx = 0;
  s_queue_count     = 0;

  /* Reset send tracking */
  s_last_send_channel     = k_comm_channel_uart;
  s_last_send_type        = k_frame_type_ping;
  s_last_send_payload_len = 0;
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(s_last_send_payload, 0, sizeof(s_last_send_payload));

  /* Reset channel ready status */
  for (uint32_t i = 0; i < k_comm_channel_count; i++) {
    s_channel_ready[i] = true;
  }

  s_current_manager = nullptr;
}

void mock_comm_manager_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

void mock_comm_manager_set_deinit_return(rx_err_t err)
{
  s_deinit_return = err;
}

void mock_comm_manager_set_poll_return(rx_err_t err)
{
  s_poll_return = err;
}

void mock_comm_manager_set_send_return(rx_err_t err)
{
  s_send_return = err;
}

bool mock_comm_manager_queue_frame(rx_comm_channel_t channel,
                                   rx_frame_type_t   type,
                                   const uint8_t*    payload,
                                   uint32_t          len)
{
  mock_queued_frame_t* entry;

  if (s_queue_count >= k_mock_comm_frame_queue_size) {
    return false;
  }

  if (len > k_mock_comm_max_payload_size) {
    return false;
  }

  entry                        = &s_frame_queue[s_queue_write_idx];
  entry->channel               = channel;
  entry->frame.header.type     = type;
  entry->frame.header.length   = (uint16_t)len;
  entry->frame.header.sequence = (uint16_t)s_queue_write_idx;
  entry->frame.header.flags    = 0;

  if (payload != nullptr && len > 0) {
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memcpy(entry->frame.payload, payload, len);
  }

  entry->valid = true;

  s_queue_write_idx = (s_queue_write_idx + 1) % k_mock_comm_frame_queue_size;
  s_queue_count++;

  /* Set poll return to ok since we have frames */
  s_poll_return = k_rx_ok;

  return true;
}

void mock_comm_manager_set_channel_ready(rx_comm_channel_t channel, bool ready)
{
  if (channel < k_comm_channel_count) {
    s_channel_ready[channel] = ready;
  }
}

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_comm_manager_get_init_count(void)
{
  return s_init_count;
}

uint32_t mock_comm_manager_get_deinit_count(void)
{
  return s_deinit_count;
}

uint32_t mock_comm_manager_get_poll_count(void)
{
  return s_poll_count;
}

uint32_t mock_comm_manager_get_send_count(void)
{
  return s_send_count;
}

rx_comm_channel_t mock_comm_manager_get_last_send_channel(void)
{
  return s_last_send_channel;
}

rx_frame_type_t mock_comm_manager_get_last_send_type(void)
{
  return s_last_send_type;
}

uint32_t mock_comm_manager_get_last_send_payload(uint8_t* out_payload, uint32_t max_len)
{
  uint32_t copy_len;

  if (out_payload == nullptr || max_len == 0) {
    return 0;
  }

  copy_len = s_last_send_payload_len;
  if (copy_len > max_len) {
    copy_len = max_len;
  }

  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memcpy(out_payload, s_last_send_payload, copy_len);

  return copy_len;
}

bool mock_comm_manager_was_initialized(void)
{
  return (bool)(s_init_count > 0);
}

uint32_t mock_comm_manager_get_queue_count(void)
{
  return s_queue_count;
}

/* =============================================================================
 * Mock Implementation
 * =============================================================================
 */

rx_err_t rx_comm_manager_init(rx_comm_manager_t* mgr, const rx_comm_manager_config_t* cfg)
{
  s_init_count++;

  if (mgr == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_init_return == k_rx_ok) {
    if (cfg != nullptr) {
      mgr->uart_handle           = cfg->uart_handle;
      mgr->spi_handle            = cfg->spi_handle;
      mgr->callback              = cfg->callback;
      mgr->callback_ctx          = cfg->callback_ctx;
      mgr->enable_decoded_output = cfg->enable_decoded_output;
    } else {
      mgr->uart_handle           = nullptr;
      mgr->spi_handle            = nullptr;
      mgr->callback              = nullptr;
      mgr->callback_ctx          = nullptr;
      mgr->enable_decoded_output = false;
    }
    mgr->initialized  = true;
    s_current_manager = mgr;
  }

  return s_init_return;
}

rx_err_t rx_comm_manager_deinit(rx_comm_manager_t* mgr)
{
  s_deinit_count++;

  if (mgr == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_deinit_return == k_rx_ok) {
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(mgr, 0, sizeof(*mgr));
    mgr->initialized  = false;
    s_current_manager = nullptr;
  }

  return s_deinit_return;
}

rx_err_t rx_comm_manager_poll(rx_comm_manager_t* mgr)
{
  mock_queued_frame_t* entry;

  s_poll_count++;

  if (mgr == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (!mgr->initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check if we have queued frames to deliver */
  if (s_queue_count > 0) {
    entry = &s_frame_queue[s_queue_read_idx];

    /* NOLINTNEXTLINE(readability-implicit-bool-conversion) */
    if (entry->valid && mgr->callback != nullptr) {
      /* Invoke callback with queued frame */
      mgr->callback(entry->channel, &entry->frame, mgr->callback_ctx);
    }

    /* Remove from queue */
    entry->valid     = false;
    s_queue_read_idx = (s_queue_read_idx + 1) % k_mock_comm_frame_queue_size;
    s_queue_count--;

    return k_rx_ok;
  }

  return s_poll_return;
}

rx_err_t rx_comm_manager_send(rx_comm_manager_t* mgr, const rx_comm_send_params_t* params)
{
  s_send_count++;

  if (mgr == nullptr || params == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (!mgr->initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_send_return == k_rx_ok) {
    /* Track send parameters */
    s_last_send_channel = params->channel;
    s_last_send_type    = params->type;

    if (params->payload != nullptr && params->payload_len > 0) {
      uint32_t copy_len = params->payload_len;
      if (copy_len > k_mock_comm_max_payload_size) {
        copy_len = k_mock_comm_max_payload_size;
      }
      /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
      memcpy(s_last_send_payload, params->payload, copy_len);
      s_last_send_payload_len = copy_len;
    } else {
      s_last_send_payload_len = 0;
    }
  }

  return s_send_return;
}

rx_err_t rx_comm_manager_respond(rx_comm_manager_t* mgr,
                                 rx_comm_channel_t  channel,
                                 const uint8_t*     payload,
                                 uint32_t           payload_len)
{
  rx_comm_send_params_t params;

  s_respond_count++;

  /* Build params and delegate to send */
  params.channel     = channel;
  params.type        = k_frame_type_response;
  params.flags       = 0;
  params.payload     = payload;
  params.payload_len = payload_len;

  return rx_comm_manager_send(mgr, &params);
}

rx_err_t rx_comm_manager_stream_send(rx_comm_manager_t* mgr, const rx_comm_send_params_t* params)
{
  return rx_comm_manager_send(mgr, params);
}

rx_err_t rx_comm_manager_event_send(rx_comm_manager_t* mgr, const rx_comm_send_params_t* params)
{
  return rx_comm_manager_send(mgr, params);
}

rx_err_t
rx_comm_manager_channel_ready(rx_comm_manager_t* mgr, rx_comm_channel_t channel, bool* ready)
{
  if (mgr == nullptr || ready == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (channel >= k_comm_channel_count) {
    return k_rx_err_invalid_arg;
  }

  *ready = s_channel_ready[channel];

  return k_rx_ok;
}

const char* rx_comm_manager_channel_name(rx_comm_channel_t channel)
{
  switch (channel) {
    case k_comm_channel_uart:
      return "USB";
    case k_comm_channel_spi:
      return "SPI";
    default:
      return "UNKNOWN";
  }
}
