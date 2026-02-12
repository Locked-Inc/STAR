/* tests/mocks/mock_sci_spi.h */

/**
 * @file mock_sci_spi.h
 * @brief Mock SCI SPI controller driver for unit testing
 *
 * @details
 * Provides test double for the SCI SPI controller driver to enable
 * unit testing of DRV8243 SPI communication without RX72N hardware.
 * Supports TX data capture, RX data injection, error injection,
 * and call tracking.
 *
 * @see rx_sci_spi.h Real SCI SPI controller driver
 * @see tests/test_rx_drv8243_spi.c DRV8243 SPI test suite
 *
 * @author STAR Team
 * @date 2026-02-11
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_port_constants.h"
#include "rx_sci_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/** @brief Mock SCI SPI constants */
typedef enum : uint16_t {
  k_mock_sci_spi_max_channels  = 1,    /**< Only channel 12 (index 0) */
  k_mock_sci_spi_tx_history_sz = 2048, /**< TX history buffer size */
} mock_sci_spi_constants_t;

/* =============================================================================
 * Mock Channel State
 * =============================================================================
 */

/** @brief State of a single SCI SPI controller channel */
typedef struct {
  bool     initialized;                                /**< Channel initialized */
  uint8_t  spi_mode;                                   /**< Configured SPI mode (0-3) */
  uint32_t freq_hz;                                    /**< Configured clock frequency */
  uint8_t  cs_port;                                    /**< Chip select port */
  uint8_t  cs_pin;                                     /**< Chip select pin */
  uint16_t last_tx_data;                               /**< Last transmitted 16-bit data */
  uint16_t next_rx_data;                               /**< Next data to return on transfer */
  uint8_t  tx_history[k_mock_sci_spi_tx_history_sz];   /**< TX history buffer */
  uint32_t tx_history_len;                             /**< TX history length */
} mock_sci_spi_channel_t;

/* =============================================================================
 * Mock State Structure
 * =============================================================================
 */

/** @brief Mock SCI SPI state */
typedef struct {
  mock_sci_spi_channel_t channels[k_mock_sci_spi_max_channels]; /**< Channel states */

  /* Configurable return values for error injection */
  rx_err_t next_init_return;     /**< Return for next init call */
  rx_err_t next_transfer_return; /**< Return for next transfer call */
  rx_err_t next_deinit_return;   /**< Return for next deinit call */

  /* Statistics */
  uint32_t init_calls;     /**< Number of init calls */
  uint32_t transfer_calls; /**< Number of transfer calls */
  uint32_t deinit_calls;   /**< Number of deinit calls */
} mock_sci_spi_t;

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

extern mock_sci_spi_t g_mock_sci_spi;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Initialize the mock SCI SPI (reset all state)
 */
void mock_sci_spi_init(void);

/**
 * @brief Set return value for next sci_spi_init_controller call
 * @param[in] ret Return value
 */
void mock_sci_spi_set_init_return(rx_err_t ret);

/**
 * @brief Set return value for next sci_spi_controller_transfer_16bit call
 * @param[in] ret Return value
 */
void mock_sci_spi_set_transfer_return(rx_err_t ret);

/**
 * @brief Set return value for next sci_spi_controller_deinit call
 * @param[in] ret Return value
 */
void mock_sci_spi_set_deinit_return(rx_err_t ret);

/**
 * @brief Set the data that will be returned by the next transfer
 * @param[in] channel SCI channel number (12)
 * @param[in] rx_data 16-bit data to return
 */
void mock_sci_spi_set_rx_data(uint8_t channel, uint16_t rx_data);

/**
 * @brief Get the last 16-bit data transmitted
 * @param[in] channel SCI channel number (12)
 * @return Last transmitted 16-bit data
 */
uint16_t mock_sci_spi_get_last_tx(uint8_t channel);

/**
 * @brief Clear channel state
 * @param[in] channel SCI channel number (12)
 */
void mock_sci_spi_clear_channel(uint8_t channel);

#ifdef __cplusplus
}
#endif
