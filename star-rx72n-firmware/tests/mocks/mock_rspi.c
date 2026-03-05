/**
 * @file mock_rspi.c
 * @brief Mock RSPI Hardware Layer Implementation for Host-Side Testing
 *
 * Mock implementation of the RSPI (SPI) hardware layer.
 * Provides call tracking, error injection, and data injection capabilities
 * for testing the SPI communication layer.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_rspi.h"

#include <string.h>

#include "rx_port_constants.h"

/* =============================================================================
 * Byte Extraction Constants
 * =============================================================================
 */

/** @brief Constants for 16-bit to byte conversion */
typedef enum : uint8_t {
  k_byte_shift_high = 8,   /**< Bit shift to extract high byte from 16-bit value */
  k_byte_mask_low   = 0xFF /**< Mask to extract low byte from 16-bit value */
} byte_extraction_t;

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
  return (mock != nullptr) ? mock : &g_mock_rspi;
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

  /* Set default controller mode return values */
  m->next_controller_init_return     = k_rx_ok;
  m->next_controller_transfer_return = k_rx_ok;
  m->next_controller_cs_return       = k_rx_ok;
  m->next_controller_deinit_return   = k_rx_ok;

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

void mock_rspi_set_controller_init_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m                 = internal_get_mock(mock);
  m->next_controller_init_return = ret;
}

void mock_rspi_set_controller_transfer_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m                     = internal_get_mock(mock);
  m->next_controller_transfer_return = ret;
}

void mock_rspi_set_controller_cs_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m               = internal_get_mock(mock);
  m->next_controller_cs_return = ret;
}

void mock_rspi_set_controller_deinit_return(mock_rspi_t* mock, rx_err_t ret)
{
  mock_rspi_t* m                   = internal_get_mock(mock);
  m->next_controller_deinit_return = ret;
}

void mock_rspi_set_controller_rx_data(mock_rspi_t* mock, rspi_channel_t channel, uint16_t rx_data)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if ((uint8_t)channel < k_mock_rspi_max_channels) {
    m->controller[channel].next_rx_data = rx_data;
  }
}

uint16_t mock_rspi_get_controller_last_tx(mock_rspi_t* mock, rspi_channel_t channel)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if ((uint8_t)channel < k_mock_rspi_max_channels) {
    return m->controller[channel].last_tx_data;
  }
  return 0;
}

void mock_rspi_clear_controller_channel(mock_rspi_t* mock, rspi_channel_t channel)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if ((uint8_t)channel < k_mock_rspi_max_channels) {
    memset(&m->controller[channel], 0, sizeof(mock_rspi_controller_t));
  }
}

/* =============================================================================
 * Data Injection/Extraction Functions
 * =============================================================================
 */

rx_err_t mock_rspi_inject_rx_data(mock_rspi_t*   mock,
                                  rspi_channel_t channel,
                                  const uint8_t* data,
                                  uint32_t       len)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (data == nullptr && len > 0) {
    return k_rx_err_invalid_arg;
  }

  if (len > k_mock_rspi_buffer_size) {
    return k_rx_err_invalid_size;
  }

  mock_rspi_channel_t* ch = &m->channels[channel];

  if (data != nullptr && len > 0) {
    memcpy(ch->rx_data, data, len);
  }
  ch->rx_len         = len;
  ch->rx_pos         = 0;
  ch->data_available = (len > 0);

  return k_rx_ok;
}

rx_err_t mock_rspi_get_tx_data(mock_rspi_t*   mock,
                               rspi_channel_t channel,
                               uint8_t*       data,
                               uint32_t       max_len,
                               uint32_t*      actual_len)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    return k_rx_err_invalid_arg;
  }

  if (data == nullptr || actual_len == nullptr) {
    return k_rx_err_invalid_arg;
  }

  mock_rspi_channel_t* ch = &m->channels[channel];

  uint32_t copy_len = (ch->tx_len < max_len) ? ch->tx_len : max_len;
  memcpy(data, ch->tx_data, copy_len);
  *actual_len = copy_len;

  return k_rx_ok;
}

void mock_rspi_set_data_available(mock_rspi_t* mock, rspi_channel_t channel, bool available)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if ((uint8_t)channel < k_mock_rspi_max_channels) {
    m->channels[channel].data_available = available;
  }
}

void mock_rspi_set_write_ready(mock_rspi_t* mock, rspi_channel_t channel, bool ready)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if ((uint8_t)channel < k_mock_rspi_max_channels) {
    m->channels[channel].write_ready = ready;
  }
}

void mock_rspi_clear_channel(mock_rspi_t* mock, rspi_channel_t channel)
{
  mock_rspi_t* m = internal_get_mock(mock);

  if ((uint8_t)channel < k_mock_rspi_max_channels) {
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

  if (out_call == nullptr || func == nullptr) {
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

rx_err_t rspi_init_peripheral(rspi_channel_t channel, const rspi_config_t* config)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->init_calls++;

  if (config == nullptr) {
    rx_err_t ret = k_rx_err_null_ptr;
    mock_rspi_record_call(mock, "rspi_init_peripheral", channel, 0, 0, ret);
    return ret;
  }

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(mock,
                          "rspi_init_peripheral",
                          channel,
                          config->spi_mode,
                          config->use_16bit,
                          ret);
    return ret;
  }

  rx_err_t ret = mock->next_init_return;

  if (ret == k_rx_ok) {
    mock->channels[channel].initialized = true;
    mock->channels[channel].spi_mode    = config->spi_mode;
    mock->channels[channel].use_16bit   = config->use_16bit;
    mock->channels[channel].write_ready = true;
  }

  mock_rspi_record_call(mock,
                        "rspi_init_peripheral",
                        channel,
                        config->spi_mode,
                        config->use_16bit,
                        ret);

  /* Reset to default for next call */
  mock->next_init_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_peripheral_transfer(rspi_channel_t channel,
                                  const uint8_t* tx_data,
                                  uint8_t*       rx_data,
                                  uint16_t       length)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->transfer_calls++;

  /* Validate arguments (match production HAL behavior) */
  if (tx_data == nullptr || rx_data == nullptr) {
    rx_err_t ret = k_rx_err_null_ptr;
    mock_rspi_record_call(mock, "rspi_peripheral_transfer", channel, length, 0, ret);
    return ret;
  }

  if (length == 0) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(mock, "rspi_peripheral_transfer", channel, length, 0, ret);
    return ret;
  }

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_peripheral_transfer", channel, length, 0, ret);
    return ret;
  }

  mock_rspi_channel_t* ch = &mock->channels[channel];

  if (!ch->initialized) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_peripheral_transfer", channel, length, 0, ret);
    return ret;
  }

  rx_err_t ret = mock->next_transfer_return;

  if (ret == k_rx_ok) {
    /* Capture TX data */
    uint32_t copy_len = (length < k_mock_rspi_buffer_size) ? length : k_mock_rspi_buffer_size;
    (void)memcpy(ch->tx_data, tx_data, copy_len);
    ch->tx_len = copy_len;

    /* Provide RX data from injected buffer */
    uint32_t avail   = (ch->rx_len > ch->rx_pos) ? (ch->rx_len - ch->rx_pos) : 0;
    uint32_t rx_copy = (length < avail) ? length : avail;

    if (rx_copy > 0) {
      (void)memcpy(rx_data, &ch->rx_data[ch->rx_pos], rx_copy);
      ch->rx_pos += rx_copy;
    }

    /* Zero-fill any remaining bytes if not enough RX data */
    if (rx_copy < length) {
      (void)memset(rx_data + rx_copy, 0, length - rx_copy);
    }

    /* Update data available flag */
    if (ch->rx_pos >= ch->rx_len) {
      ch->data_available = false;
    }
  }

  mock_rspi_record_call(mock, "rspi_peripheral_transfer", channel, length, 0, ret);

  /* Reset to default for next call */
  mock->next_transfer_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_peripheral_read_available(rspi_channel_t channel, bool* available)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->available_calls++;

  if (available == nullptr) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(mock, "rspi_peripheral_read_available", channel, 0, 0, ret);
    return ret;
  }

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_peripheral_read_available", channel, 0, 0, ret);
    return ret;
  }

  mock_rspi_channel_t* ch = &mock->channels[channel];

  if (!ch->initialized) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_peripheral_read_available", channel, 0, 0, ret);
    return ret;
  }

  rx_err_t ret = mock->next_available_return;

  if (ret == k_rx_ok) {
    *available = ch->data_available;
  }

  mock_rspi_record_call(mock, "rspi_peripheral_read_available", channel, 0, 0, ret);

  /* Reset to default for next call */
  mock->next_available_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_peripheral_write_ready(rspi_channel_t channel, bool* ready)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->ready_calls++;

  if (ready == nullptr) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(mock, "rspi_peripheral_write_ready", channel, 0, 0, ret);
    return ret;
  }

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_peripheral_write_ready", channel, 0, 0, ret);
    return ret;
  }

  mock_rspi_channel_t* ch = &mock->channels[channel];

  if (!ch->initialized) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_peripheral_write_ready", channel, 0, 0, ret);
    return ret;
  }

  rx_err_t ret = mock->next_ready_return;

  if (ret == k_rx_ok) {
    *ready = ch->write_ready;
  }

  mock_rspi_record_call(mock, "rspi_peripheral_write_ready", channel, 0, 0, ret);

  /* Reset to default for next call */
  mock->next_ready_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_deinit(rspi_channel_t channel)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->deinit_calls++;

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(mock, "rspi_deinit", channel, 0, 0, ret);
    return ret;
  }

  rx_err_t ret = mock->next_deinit_return;

  if (ret == k_rx_ok) {
    mock_rspi_channel_t* ch = &mock->channels[channel];

    ch->initialized    = false;
    ch->data_available = false;
    ch->write_ready    = false;
  }

  mock_rspi_record_call(mock, "rspi_deinit", channel, 0, 0, ret);

  /* Reset to default for next call */
  mock->next_deinit_return = k_rx_ok;

  return ret;
}

/* =============================================================================
 * Mock Controller Mode HAL Function Implementations
 * =============================================================================
 */

rx_err_t rspi_init_controller(rspi_channel_t channel, const rspi_controller_config_t* config)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->controller_init_calls++;

  if (config == nullptr) {
    rx_err_t ret = k_rx_err_null_ptr;
    mock_rspi_record_call(mock, "rspi_init_controller", channel, 0, 0, ret);
    return ret;
  }

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(mock, "rspi_init_controller", channel, config->freq_hz, 0, ret);
    return ret;
  }

  rx_err_t ret = mock->next_controller_init_return;

  if (ret == k_rx_ok) {
    mock_rspi_controller_t* ctrl = &mock->controller[channel];
    ctrl->initialized            = true;
    ctrl->spi_mode               = config->spi_mode;
    ctrl->freq_hz                = config->freq_hz;
    ctrl->cs_port                = rx_port_from_pin(config->cs);
    ctrl->cs_pin                 = rx_pin_from_pin(config->cs);
    ctrl->cs_active              = false;
  }

  mock_rspi_record_call(mock,
                        "rspi_init_controller",
                        channel,
                        config->freq_hz,
                        config->spi_mode,
                        ret);

  /* Reset to default for next call */
  mock->next_controller_init_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_controller_set_cs(rspi_channel_t channel, bool active)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->controller_cs_calls++;

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_controller_set_cs", channel, active, 0, ret);
    return ret;
  }

  mock_rspi_controller_t* ctrl = &mock->controller[channel];

  if (!ctrl->initialized) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_controller_set_cs", channel, active, 0, ret);
    return ret;
  }

  rx_err_t ret = mock->next_controller_cs_return;

  if (ret == k_rx_ok) {
    ctrl->cs_active = active;
  }

  mock_rspi_record_call(mock, "rspi_controller_set_cs", channel, active, 0, ret);

  /* Reset to default for next call */
  mock->next_controller_cs_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_controller_transfer_16bit(rspi_channel_t channel, uint16_t tx_data, uint16_t* rx_data)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->controller_transfer_calls++;

  /* Validate arguments (match production HAL behavior) */
  if (rx_data == nullptr) {
    rx_err_t ret = k_rx_err_null_ptr;
    mock_rspi_record_call(mock, "rspi_controller_transfer_16bit", channel, tx_data, 0, ret);
    return ret;
  }

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_controller_transfer_16bit", channel, tx_data, 0, ret);
    return ret;
  }

  mock_rspi_controller_t* ctrl = &mock->controller[channel];

  if (!ctrl->initialized) {
    rx_err_t ret = k_rx_err_invalid_state;
    mock_rspi_record_call(mock, "rspi_controller_transfer_16bit", channel, tx_data, 0, ret);
    return ret;
  }

  rx_err_t ret = mock->next_controller_transfer_return;

  if (ret == k_rx_ok) {
    /* Store transmitted data */
    ctrl->last_tx_data = tx_data;

    /* Store in history buffer */
    if (ctrl->tx_history_len + 2 <= k_mock_rspi_buffer_size) {
      ctrl->tx_history[ctrl->tx_history_len++] = (uint8_t)(tx_data >> k_byte_shift_high);
      ctrl->tx_history[ctrl->tx_history_len++] = (uint8_t)(tx_data & k_byte_mask_low);
    }

    /* Provide RX data */
    *rx_data = ctrl->next_rx_data;
  }

  mock_rspi_record_call(mock,
                        "rspi_controller_transfer_16bit",
                        channel,
                        tx_data,
                        ctrl->next_rx_data,
                        ret);

  /* Reset to default for next call */
  mock->next_controller_transfer_return = k_rx_ok;

  return ret;
}

rx_err_t rspi_controller_deinit(rspi_channel_t channel)
{
  mock_rspi_t* mock = &g_mock_rspi;

  mock->controller_deinit_calls++;

  if ((uint8_t)channel >= k_mock_rspi_max_channels) {
    rx_err_t ret = k_rx_err_invalid_arg;
    mock_rspi_record_call(mock, "rspi_controller_deinit", channel, 0, 0, ret);
    return ret;
  }

  rx_err_t ret = mock->next_controller_deinit_return;

  if (ret == k_rx_ok) {
    memset(&mock->controller[channel], 0, sizeof(mock_rspi_controller_t));
  }

  mock_rspi_record_call(mock, "rspi_controller_deinit", channel, 0, 0, ret);

  /* Reset to default for next call */
  mock->next_controller_deinit_return = k_rx_ok;

  return ret;
}
