/* tests/mocks/mock_riic_hal.c */

/**
 * @file mock_riic_hal.c
 * @brief Mock RIIC (I2C) HAL Function Implementation
 *
 * Provides mock implementations of RIIC HAL functions for unit testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_riic_hal.h"

#include <string.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Valid I2C frequencies */
typedef enum : uint32_t {
  k_mock_riic_freq_100khz = 100000,  /**< Standard mode */
  k_mock_riic_freq_400khz = 400000,  /**< Fast mode */
  k_mock_riic_freq_1mhz   = 1000000, /**< Fast mode plus */
} mock_riic_frequency_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

mock_riic_state_t g_mock_riic;

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Record a call in the history
 */
static void internal_record_call(mock_riic_call_type_t type,
                                 uint8_t               channel,
                                 uint8_t               device_addr,
                                 uint16_t              write_length,
                                 uint16_t              read_length)
{
  if (g_mock_riic.call_count < k_mock_riic_call_history_size) {
    mock_riic_call_t* call = &g_mock_riic.call_history[g_mock_riic.call_count];
    call->type             = type;
    call->channel          = channel;
    call->device_addr      = device_addr;
    call->write_length     = write_length;
    call->read_length      = read_length;
    g_mock_riic.call_count++;
  }
}

/**
 * @brief Check and consume error injection
 */
static rx_err_t internal_check_error(void)
{
  if (g_mock_riic.error_set) {
    rx_err_t err          = g_mock_riic.next_error;
    g_mock_riic.error_set = false;
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Check for simulated error conditions
 */
static rx_err_t internal_check_simulated_errors(void)
{
  if (g_mock_riic.simulate_busy) {
    return k_rx_err_timeout; /* Bus busy appears as timeout */
  }
  if (g_mock_riic.simulate_timeout) {
    return k_rx_err_timeout;
  }
  if (g_mock_riic.simulate_nack) {
    return k_rx_err_nack;
  }
  return k_rx_ok;
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

void mock_riic_init(void)
{
  memset(&g_mock_riic, 0, sizeof(g_mock_riic));
}

void mock_riic_reset(void)
{
  mock_riic_init();
}

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

void mock_riic_set_rx_data(uint8_t channel, const uint8_t* data, uint16_t length)
{
  if (channel >= k_mock_riic_max_channels || data == nullptr) {
    return;
  }

  uint16_t to_copy = (length < k_mock_riic_buffer_size) ? length : k_mock_riic_buffer_size;
  memcpy(g_mock_riic.channels[channel].rx_buffer, data, to_copy);
  g_mock_riic.channels[channel].rx_length = to_copy;
}

void mock_riic_simulate_nack(bool simulate)
{
  g_mock_riic.simulate_nack = simulate;
}

void mock_riic_simulate_timeout(bool simulate)
{
  g_mock_riic.simulate_timeout = simulate;
}

void mock_riic_simulate_busy(bool simulate)
{
  g_mock_riic.simulate_busy = simulate;
}

void mock_riic_set_next_error(rx_err_t err)
{
  g_mock_riic.next_error = err;
  g_mock_riic.error_set  = true;
}

void mock_riic_clear_error(void)
{
  g_mock_riic.error_set = false;
}

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

bool mock_riic_is_initialized(uint8_t channel)
{
  if (channel < k_mock_riic_max_channels) {
    return g_mock_riic.channels[channel].initialized;
  }
  return false;
}

uint32_t mock_riic_get_frequency(uint8_t channel)
{
  if (channel < k_mock_riic_max_channels) {
    return g_mock_riic.channels[channel].frequency_hz;
  }
  return 0;
}

uint16_t mock_riic_get_tx_data(uint8_t channel, uint8_t* data, uint16_t max_length)
{
  if (channel >= k_mock_riic_max_channels || data == nullptr) {
    return 0;
  }

  mock_riic_channel_state_t* ch      = &g_mock_riic.channels[channel];
  uint16_t                   to_copy = (max_length < ch->tx_length) ? max_length : ch->tx_length;
  memcpy(data, ch->tx_buffer, to_copy);
  return to_copy;
}

uint8_t mock_riic_get_last_device_addr(uint8_t channel)
{
  if (channel < k_mock_riic_max_channels) {
    return g_mock_riic.channels[channel].last_device_addr;
  }
  return 0;
}

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

const mock_riic_call_t* mock_riic_get_call(uint16_t index)
{
  if (index < g_mock_riic.call_count) {
    return &g_mock_riic.call_history[index];
  }
  return nullptr;
}

uint16_t mock_riic_get_call_count(void)
{
  return g_mock_riic.call_count;
}

void mock_riic_clear_history(void)
{
  g_mock_riic.call_count = 0;
}

/* =============================================================================
 * Mock RIIC HAL Functions
 * =============================================================================
 */

rx_err_t riic_init(riic_channel_t channel, uint32_t frequency_hz)
{
  internal_record_call(k_mock_riic_call_init, channel.value, 0, 0, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Validate channel */
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Validate frequency */
  if (frequency_hz != k_mock_riic_freq_100khz && frequency_hz != k_mock_riic_freq_400khz &&
      frequency_hz != k_mock_riic_freq_1mhz) {
    return k_rx_err_invalid_arg;
  }

  /* Initialize channel */
  g_mock_riic.channels[channel.value].initialized  = true;
  g_mock_riic.channels[channel.value].frequency_hz = frequency_hz;

  return k_rx_ok;
}

rx_err_t riic_write(riic_channel_t    channel,
                    i2c_device_addr_t device_addr,
                    const uint8_t*    data,
                    const uint16_t    length)
{
  internal_record_call(k_mock_riic_call_write, channel.value, device_addr.value, length, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer check */
  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate channel */
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!g_mock_riic.channels[channel.value].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for simulated errors */
  err = internal_check_simulated_errors();
  if (err != k_rx_ok) {
    return err;
  }

  /* Store transmitted data */
  mock_riic_channel_state_t* ch = &g_mock_riic.channels[channel.value];
  uint16_t to_copy = (length < k_mock_riic_buffer_size) ? length : k_mock_riic_buffer_size;
  memcpy(ch->tx_buffer, data, to_copy);
  ch->tx_length        = to_copy;
  ch->last_device_addr = device_addr.value;

  return k_rx_ok;
}

rx_err_t riic_read(riic_channel_t    channel,
                   i2c_device_addr_t device_addr,
                   uint8_t*          data,
                   const uint16_t    length)
{
  internal_record_call(k_mock_riic_call_read, channel.value, device_addr.value, 0, length);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer check */
  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate channel */
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!g_mock_riic.channels[channel.value].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for simulated errors */
  err = internal_check_simulated_errors();
  if (err != k_rx_ok) {
    return err;
  }

  /* Copy pre-loaded RX data */
  mock_riic_channel_state_t* ch      = &g_mock_riic.channels[channel.value];
  uint16_t                   to_copy = (length < ch->rx_length) ? length : ch->rx_length;
  memcpy(data, ch->rx_buffer, to_copy);
  ch->last_device_addr = device_addr.value;

  return k_rx_ok;
}

rx_err_t riic_write_read(riic_channel_t    channel,
                         i2c_device_addr_t device_addr,
                         const uint8_t*    write_data,
                         uint16_t          write_length,
                         uint8_t*          read_data,
                         uint16_t          read_length)
{
  internal_record_call(k_mock_riic_call_write_read,
                       channel.value,
                       device_addr.value,
                       write_length,
                       read_length);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer checks */
  if (write_data == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (read_data == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate channel */
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!g_mock_riic.channels[channel.value].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for simulated errors */
  err = internal_check_simulated_errors();
  if (err != k_rx_ok) {
    return err;
  }

  /* Store transmitted data */
  mock_riic_channel_state_t* ch = &g_mock_riic.channels[channel.value];
  uint16_t                   to_write =
    (write_length < k_mock_riic_buffer_size) ? write_length : k_mock_riic_buffer_size;
  memcpy(ch->tx_buffer, write_data, to_write);
  ch->tx_length        = to_write;
  ch->last_device_addr = device_addr.value;

  /* Copy pre-loaded RX data */
  uint16_t to_read = (read_length < ch->rx_length) ? read_length : ch->rx_length;
  memcpy(read_data, ch->rx_buffer, to_read);

  return k_rx_ok;
}
