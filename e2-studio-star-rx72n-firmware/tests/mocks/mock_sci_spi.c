/* tests/mocks/mock_sci_spi.c */

/**
 * @file mock_sci_spi.c
 * @brief Mock SCI SPI controller driver implementation for unit testing
 *
 * @details
 * Provides mock implementations of sci_spi_init_controller(),
 * sci_spi_controller_transfer_16bit(), and sci_spi_controller_deinit()
 * for host-side testing of DRV8243 SPI communication.
 *
 * @date 2026-02-11
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_sci_spi.h"

#include <string.h>

#include "rx_port_constants.h"
#include "rx_sci_spi.h"

/* =============================================================================
 * Byte Extraction Constants
 * =============================================================================
 */

/** @brief Constants for 16-bit to byte conversion */
typedef enum : uint8_t {
  k_byte_shift_high = 8,   /**< Bit shift to extract high byte */
  k_byte_mask_low   = 0xFF /**< Mask to extract low byte */
} byte_extraction_t;

/** @brief SCI12 channel mapping constant */
typedef enum : uint8_t {
  k_sci12_channel = 12, /**< SCI12 channel number */
} sci_channel_constants_t;

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

mock_sci_spi_t g_mock_sci_spi;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Map external channel number to internal index
 * @param[in]  channel SCI channel number
 * @param[out] idx     Internal index
 * @return true if valid channel
 */
static bool internal_channel_to_index(uint8_t channel, uint8_t* idx)
{
  if (channel == k_sci12_channel) {
    *idx = 0;
    return true;
  }
  return false;
}

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_sci_spi_init(void)
{
  memset(&g_mock_sci_spi, 0, sizeof(mock_sci_spi_t));

  g_mock_sci_spi.next_init_return     = k_rx_ok;
  g_mock_sci_spi.next_transfer_return = k_rx_ok;
  g_mock_sci_spi.next_deinit_return   = k_rx_ok;
}

void mock_sci_spi_set_init_return(rx_err_t ret)
{
  g_mock_sci_spi.next_init_return = ret;
}

void mock_sci_spi_set_transfer_return(rx_err_t ret)
{
  g_mock_sci_spi.next_transfer_return = ret;
}

void mock_sci_spi_set_deinit_return(rx_err_t ret)
{
  g_mock_sci_spi.next_deinit_return = ret;
}

void mock_sci_spi_set_rx_data(uint8_t channel, uint16_t rx_data)
{
  uint8_t idx;
  if (internal_channel_to_index(channel, &idx)) {
    g_mock_sci_spi.channels[idx].next_rx_data = rx_data;
  }
}

uint16_t mock_sci_spi_get_last_tx(uint8_t channel)
{
  uint8_t idx;
  if (internal_channel_to_index(channel, &idx)) {
    return g_mock_sci_spi.channels[idx].last_tx_data;
  }
  return 0;
}

void mock_sci_spi_clear_channel(uint8_t channel)
{
  uint8_t idx;
  if (internal_channel_to_index(channel, &idx)) {
    memset(&g_mock_sci_spi.channels[idx], 0, sizeof(mock_sci_spi_channel_t));
  }
}

/* =============================================================================
 * Mock HAL Function Implementations
 * (These replace the real sci_spi.c functions during testing)
 * =============================================================================
 */

rx_err_t sci_spi_init_controller(uint8_t channel, const sci_spi_controller_config_t* config)
{
  uint8_t idx;

  g_mock_sci_spi.init_calls++;

  if (config == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (!internal_channel_to_index(channel, &idx)) {
    return k_rx_err_invalid_arg;
  }

  rx_err_t ret = g_mock_sci_spi.next_init_return;

  if (ret == k_rx_ok) {
    mock_sci_spi_channel_t* ch = &g_mock_sci_spi.channels[idx];
    ch->initialized            = true;
    ch->spi_mode               = config->spi_mode;
    ch->freq_hz                = config->freq_hz;
    ch->cs_port                = rx_port_from_pin(config->cs);
    ch->cs_pin                 = rx_pin_from_pin(config->cs);
  }

  /* Reset to default for next call */
  g_mock_sci_spi.next_init_return = k_rx_ok;

  return ret;
}

rx_err_t sci_spi_controller_transfer_16bit(uint8_t channel, uint16_t tx_data, uint16_t* rx_data)
{
  uint8_t idx;

  g_mock_sci_spi.transfer_calls++;

  if (!internal_channel_to_index(channel, &idx)) {
    return k_rx_err_invalid_arg;
  }

  mock_sci_spi_channel_t* ch = &g_mock_sci_spi.channels[idx];

  if (!ch->initialized) {
    return k_rx_err_invalid_state;
  }

  rx_err_t ret = g_mock_sci_spi.next_transfer_return;

  if (ret == k_rx_ok) {
    /* Store transmitted data */
    ch->last_tx_data = tx_data;

    /* Store in history buffer */
    if (ch->tx_history_len + 2 <= k_mock_sci_spi_tx_history_sz) {
      ch->tx_history[ch->tx_history_len++] = (uint8_t)(tx_data >> k_byte_shift_high);
      ch->tx_history[ch->tx_history_len++] = (uint8_t)(tx_data & k_byte_mask_low);
    }

    /* Provide RX data */
    if (rx_data != nullptr) {
      *rx_data = ch->next_rx_data;
    }
  }

  /* Reset to default for next call */
  g_mock_sci_spi.next_transfer_return = k_rx_ok;

  return ret;
}

rx_err_t sci_spi_controller_deinit(uint8_t channel)
{
  uint8_t idx;

  g_mock_sci_spi.deinit_calls++;

  if (!internal_channel_to_index(channel, &idx)) {
    return k_rx_err_invalid_arg;
  }

  rx_err_t ret = g_mock_sci_spi.next_deinit_return;

  if (ret == k_rx_ok) {
    memset(&g_mock_sci_spi.channels[idx], 0, sizeof(mock_sci_spi_channel_t));
  }

  /* Reset to default for next call */
  g_mock_sci_spi.next_deinit_return = k_rx_ok;

  return ret;
}
