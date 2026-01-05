/* tests/mocks/mock_rspi.c */

/**
 * @file mock_rspi.c
 * @brief Mock RSPI Hardware Layer Implementation for Host-Side Testing
 *
 * Mock implementation of the RSPI (SPI) hardware layer.
 * Provides call tracking, error injection, and data injection capabilities
 * for testing the SPI communication layer.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rspi.h"

#include <string.h>

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

mock_rspi_t g_mock_rspi;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Get mock instance, using global if NULL provided
 */
static mock_rspi_t* internal_get_mock(mock_rspi_t* mock)
{
  return (mock != NULL) ? mock : &g_mock_rspi;
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

rx_err_t mock_rspi_init(mock_rspi_t* mock)
{
  mock_rspi_t* m = internal_get_mock(mock);

  memset(m, 0, sizeof(mock_rspi_t));

  /* Set default return values */
  m->next_init_return      = k_rx_ok;
  m->next_transfer_return  = k_rx_ok;
  m->next_available_return = k_rx_ok;
  m->next_ready_return     = k_rx_ok;
  m->next_deinit_return    = k_rx_ok;

  /* Default all channels to write ready */
  for (uint8_t i = 0; i < k_mock_rspi_max_channels; i++) {
    m->channels[i].write_ready = true;
  }

  return k_rx_ok;
}

rx_err_t mock_rspi_deinit(mock_rspi_t* mock)
{
  mock_rspi_t* m = internal_get_mock(mock);

  memset(m, 0, sizeof(mock_rspi_t));

  return k_rx_ok;
}

rx_err_t mock_rspi_clear(mock_rspi_t* mock)
{
  mock_rspi_t* m = internal_get_mock(mock);

  /* Clear channel states but preserve return value settings */
  rx_err_t saved_init      = m->next_init_return;
  rx_err_t saved_transfer  = m->next_transfer_return;
  rx_err_t saved_available = m->next_available_return;
  rx_err_t saved_ready     = m->next_ready_return;
  rx_err_t saved_deinit    = m->next_deinit_return;

  memset(m->channels, 0, sizeof(m->channels));

  /* Clear call history */
  memset(m->call_history, 0, sizeof(m->call_history));
  m->call_count       = 0;
  m->call_write_index = 0;

  /* Clear statistics */
  m->init_calls      = 0;
  m->deinit_calls    = 0;
  m->transfer_calls  = 0;
  m->available_calls = 0;
  m->ready_calls     = 0;

  /* Restore return value settings */
  m->next_init_return      = saved_init;
  m->next_transfer_return  = saved_transfer;
  m->next_available_return = saved_available;
  m->next_ready_return     = saved_ready;
  m->next_deinit_return    = saved_deinit;

  /* Default all channels to write ready */
  for (uint8_t i = 0; i < k_mock_rspi_max_channels; i++) {
    m->channels[i].write_ready = true;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

void mock_rspi_set_init_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m      = internal_get_mock(mock);
  m->next_init_return = ret;
}

void mock_rspi_set_transfer_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m          = internal_get_mock(mock);
  m->next_transfer_return = ret;
}

void mock_rspi_set_available_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m           = internal_get_mock(mock);
  m->next_available_return = ret;
}

void mock_rspi_set_ready_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m       = internal_get_mock(mock);
  m->next_ready_return = ret;
}

void mock_rspi_set_deinit_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m        = internal_get_mock(mock);
  m->next_deinit_return = ret;
}

/* =============================================================================
 * Data Injection/Extraction Functions
 * =============================================================================
 */

rx_err_t
mock_rspi_inject_rx_data(mock_rspi_t* mock, uint8_t channel, const uint8_t* data, uint32_t len)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if (channel >= k_mock_rspi_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (data == NULL && len > 0) {
    return k_rx_err_invalid_arg;
  }

  if (len > k_mock_rspi_buffer_size) {
    return k_rx_err_invalid_size;
  }

  mock_rspi_channel_t* ch = &m->channels[channel];

  if (data != NULL && len > 0) {
    memcpy(ch->rx_data, data, len);
  }
  ch->rx_len         = len;
  ch->rx_pos         = 0;
  ch->data_available = (len > 0);

  return k_rx_ok;
}

rx_err_t mock_rspi_get_tx_data(mock_rspi_t* mock,
                               uint8_t      channel,
                               uint8_t*     data,
                               uint32_t     max_len,
                               uint32_t*    actual_len)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if (channel >= k_mock_rspi_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (data == NULL || actual_len == NULL) {
    return k_rx_err_invalid_arg;
  }

  mock_rspi_channel_t* ch = &m->channels[channel];

  uint32_t copy_len = (ch->tx_len < max_len) ? ch->tx_len : max_len;
  memcpy(data, ch->tx_data, copy_len);
  *actual_len = copy_len;

  return k_rx_ok;
}

void mock_rspi_set_data_available(mock_rspi_t* mock, uint8_t channel, bool available)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if (channel < k_mock_rspi_max_channels) {
    m->channels[channel].data_available = available;
  }
}

void mock_rspi_set_write_ready(mock_rspi_t* mock, uint8_t channel, bool ready)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if (channel < k_mock_rspi_max_channels) {
    m->channels[channel].write_ready = ready;
  }
}

void mock_rspi_clear_channel(mock_rspi_t* mock, uint8_t channel)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if (channel < k_mock_rspi_max_channels) {
    mock_rspi_channel_t* ch = &m->channels[channel];

    memset(ch->rx_data, 0, sizeof(ch->rx_data));
    memset(ch->tx_data, 0, sizeof(ch->tx_data));
    ch->rx_len         = 0;
    ch->rx_pos         = 0;
    ch->tx_len         = 0;
    ch->data_available = false;
    ch->write_ready    = true;
  }
}

/* =============================================================================
 * Call Tracking Functions
 * =============================================================================
 */

bool mock_rspi_was_called(mock_rspi_t* mock, const char* func)
{
  mock_rspi_t* m = internal_get_mock(mock);

  for (uint32_t i = 0; i < m->call_count && i < k_mock_rspi_call_history_max; i++) {
    if (strncmp(m->call_history[i].function, func, k_mock_rspi_func_name_max) == 0) {
      return true;
    }
  }

  return false;
}

uint32_t mock_rspi_get_call_count(mock_rspi_t* mock, const char* func)
{
  mock_rspi_t* m     = internal_get_mock(mock);
  uint32_t     count = 0;

  for (uint32_t i = 0; i < m->call_count && i < k_mock_rspi_call_history_max; i++) {
    if (strncmp(m->call_history[i].function, func, k_mock_rspi_func_name_max) == 0) {
      count++;
    }
  }

  return count;
}

rx_err_t mock_rspi_get_last_call(mock_rspi_t* mock, const char* func, mock_rspi_call_t* out_call)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if (out_call == NULL || func == NULL) {
    return k_rx_err_invalid_arg;
  }

  /* Search backwards through call history */
  for (int32_t i = (int32_t)m->call_count - 1; i >= 0 && i < (int32_t)k_mock_rspi_call_history_max;
       i--) {
    uint32_t idx = (uint32_t)i % k_mock_rspi_call_history_max;
    if (strncmp(m->call_history[idx].function, func, k_mock_rspi_func_name_max) == 0) {
      memcpy(out_call, &m->call_history[idx], sizeof(mock_rspi_call_t));
      return k_rx_ok;
    }
  }

  return k_rx_err_not_found;
}

void mock_rspi_clear_calls(mock_rspi_t* mock)
{
  mock_rspi_t* m = internal_get_mock(mock);

  memset(m->call_history, 0, sizeof(m->call_history));
  m->call_count       = 0;
  m->call_write_index = 0;
}

void mock_rspi_record_call(mock_rspi_t* mock,
                           const char*  func,
                           uint8_t      channel,
                           uint32_t     arg1,
                           uint32_t     arg2,
                           rx_err_t     ret)
{
  mock_rspi_t* m = internal_get_mock(mock);

  mock_rspi_call_t* call = &m->call_history[m->call_write_index];

  strncpy(call->function, func, k_mock_rspi_func_name_max - 1);
  call->function[k_mock_rspi_func_name_max - 1] = '\0';
  call->channel                                 = channel;
  call->arg1                                    = arg1;
  call->arg2                                    = arg2;
  call->return_value                            = ret;

  m->call_write_index = (m->call_write_index + 1) % k_mock_rspi_call_history_max;
  m->call_count++;
}

/* =============================================================================
 * Mock HAL Function Implementations
 * =============================================================================
 */

rx_err_t rspi_init_peripheral(uint8_t channel, uint8_t mode, bool use_16bit)
{
  mock_rspi_t* m = &g_mock_rspi;

  m->init_calls++;

  if (channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(m, "rspi_init_peripheral", channel, mode, use_16bit, ret);
    return ret;
  }

  rx_err_t ret = m->next_init_return;

  if (ret == k_rx_ok) {
    m->channels[channel].initialized = true;
    m->channels[channel].spi_mode    = mode;
    m->channels[channel].use_16bit   = use_16bit;
    m->channels[channel].write_ready = true;
  }

  mock_rspi_record_call(m, "rspi_init_peripheral", channel, mode, use_16bit, ret);

  /* Reset to default for next call */
  m->next_init_return = k_rx_ok;

  return ret;
}

rx_err_t
rspi_peripheral_transfer(uint8_t channel, const uint8_t* tx_data, uint8_t* rx_data, uint16_t length)
{
  mock_rspi_t* m = &g_mock_rspi;

  m->transfer_calls++;

  if (channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(m, "rspi_peripheral_transfer", channel, length, 0, ret);
    return ret;
  }

  mock_rspi_channel_t* ch = &m->channels[channel];

  if (!ch->initialized) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(m, "rspi_peripheral_transfer", channel, length, 0, ret);
    return ret;
  }

  rx_err_t ret = m->next_transfer_return;

  if (ret == k_rx_ok) {
    /* Capture TX data */
    if (tx_data != NULL && length > 0) {
      uint32_t copy_len = (length < k_mock_rspi_buffer_size) ? length : k_mock_rspi_buffer_size;
      memcpy(ch->tx_data, tx_data, copy_len);
      ch->tx_len = copy_len;
    }

    /* Provide RX data from injected buffer */
    if (rx_data != NULL && length > 0) {
      uint32_t avail    = (ch->rx_len > ch->rx_pos) ? (ch->rx_len - ch->rx_pos) : 0;
      uint32_t copy_len = (length < avail) ? length : avail;

      if (copy_len > 0) {
        memcpy(rx_data, &ch->rx_data[ch->rx_pos], copy_len);
        ch->rx_pos += copy_len;
      }

      /* Zero-fill any remaining bytes if not enough RX data */
      if (copy_len < length) {
        memset(rx_data + copy_len, 0, length - copy_len);
      }

      /* Update data available flag */
      if (ch->rx_pos >= ch->rx_len) {
        ch->data_available = false;
      }
    }
  }

  mock_rspi_record_call(m, "rspi_peripheral_transfer", channel, length, 0, ret);

  /* Reset to default for next call */
  m->next_transfer_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_peripheral_read_available(uint8_t channel, bool* available)
{
  mock_rspi_t* m = &g_mock_rspi;

  m->available_calls++;

  if (available == NULL) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(m, "rspi_peripheral_read_available", channel, 0, 0, ret);
    return ret;
  }

  if (channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(m, "rspi_peripheral_read_available", channel, 0, 0, ret);
    return ret;
  }

  mock_rspi_channel_t* ch = &m->channels[channel];

  if (!ch->initialized) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(m, "rspi_peripheral_read_available", channel, 0, 0, ret);
    return ret;
  }

  rx_err_t ret = m->next_available_return;

  if (ret == k_rx_ok) {
    *available = ch->data_available;
  }

  mock_rspi_record_call(m, "rspi_peripheral_read_available", channel, 0, 0, ret);

  /* Reset to default for next call */
  m->next_available_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_peripheral_write_ready(uint8_t channel, bool* ready)
{
  mock_rspi_t* m = &g_mock_rspi;

  m->ready_calls++;

  if (ready == NULL) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(m, "rspi_peripheral_write_ready", channel, 0, 0, ret);
    return ret;
  }

  if (channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(m, "rspi_peripheral_write_ready", channel, 0, 0, ret);
    return ret;
  }

  mock_rspi_channel_t* ch = &m->channels[channel];

  if (!ch->initialized) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(m, "rspi_peripheral_write_ready", channel, 0, 0, ret);
    return ret;
  }

  rx_err_t ret = m->next_ready_return;

  if (ret == k_rx_ok) {
    *ready = ch->write_ready;
  }

  mock_rspi_record_call(m, "rspi_peripheral_write_ready", channel, 0, 0, ret);

  /* Reset to default for next call */
  m->next_ready_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_deinit(uint8_t channel)
{
  mock_rspi_t* m = &g_mock_rspi;

  m->deinit_calls++;

  if (channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(m, "rspi_deinit", channel, 0, 0, ret);
    return ret;
  }

  rx_err_t ret = m->next_deinit_return;

  if (ret == k_rx_ok) {
    mock_rspi_channel_t* ch = &m->channels[channel];

    ch->initialized    = false;
    ch->data_available = false;
    ch->write_ready    = false;
  }

  mock_rspi_record_call(m, "rspi_deinit", channel, 0, 0, ret);

  /* Reset to default for next call */
  m->next_deinit_return = k_rx_ok;

  return ret;
}
