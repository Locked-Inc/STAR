/* tests/mocks/mock_rspi.h */

/**
 * @file mock_rspi.h
 * @brief Mock RSPI Hardware Layer for Host-Side Testing
 *
 * Mock implementation of the RSPI (SPI) hardware layer for testing.
 * Provides call tracking, error injection, and data injection capabilities.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RSPI_H
#define MOCK_RSPI_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/** @brief Mock RSPI buffer and tracking constants */
typedef enum : uint16_t {
  k_mock_rspi_max_channels     = 3,    /**< RSPI0, RSPI1, RSPI2 */
  k_mock_rspi_buffer_size      = 2048, /**< Max transfer buffer size */
  k_mock_rspi_call_history_max = 64,   /**< Max recorded calls */
  k_mock_rspi_func_name_max    = 48,   /**< Max function name length */
} mock_rspi_constants_t;

/* =============================================================================
 * Call Record Structure
 * =============================================================================
 */

/**
 * @brief Record of a single function call
 */
typedef struct {
  char     function[k_mock_rspi_func_name_max]; /**< Function name */
  uint8_t  channel;                             /**< RSPI channel */
  uint32_t arg1;                                /**< First argument */
  uint32_t arg2;                                /**< Second argument */
  rx_err_t return_value;                        /**< Return value */
} mock_rspi_call_t;

/* =============================================================================
 * Channel State Structure
 * =============================================================================
 */

/**
 * @brief State of a single RSPI channel
 */
typedef struct {
  bool     initialized;                      /**< Channel initialized */
  uint8_t  spi_mode;                         /**< Configured SPI mode */
  bool     use_16bit;                        /**< 16-bit frame mode */
  bool     data_available;                   /**< RX data available */
  bool     write_ready;                      /**< TX ready */
  uint8_t  rx_data[k_mock_rspi_buffer_size]; /**< RX buffer for injection */
  uint32_t rx_len;                           /**< RX data length */
  uint32_t rx_pos;                           /**< RX read position */
  uint8_t  tx_data[k_mock_rspi_buffer_size]; /**< TX buffer for capture */
  uint32_t tx_len;                           /**< TX data length */
} mock_rspi_channel_t;

/* =============================================================================
 * Mock RSPI State Structure
 * =============================================================================
 */

/**
 * @brief Mock RSPI state structure
 */
typedef struct {
  /* Channel states */
  mock_rspi_channel_t channels[k_mock_rspi_max_channels]; /**< Channel states */

  /* Call tracking */
  mock_rspi_call_t call_history[k_mock_rspi_call_history_max]; /**< Call history */
  uint32_t         call_count;                                 /**< Total calls */
  uint32_t         call_write_index;                           /**< Next index */

  /* Configurable return values for error injection */
  rx_err_t next_init_return;      /**< Return for next init call */
  rx_err_t next_transfer_return;  /**< Return for next transfer call */
  rx_err_t next_available_return; /**< Return for next available call */
  rx_err_t next_ready_return;     /**< Return for next ready call */
  rx_err_t next_deinit_return;    /**< Return for next deinit call */

  /* Statistics */
  uint32_t init_calls;      /**< Number of init calls */
  uint32_t deinit_calls;    /**< Number of deinit calls */
  uint32_t transfer_calls;  /**< Number of transfer calls */
  uint32_t available_calls; /**< Number of available check calls */
  uint32_t ready_calls;     /**< Number of ready check calls */
} mock_rspi_t;

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

extern mock_rspi_t g_mock_rspi;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize the mock RSPI
 *
 * Clears all state and sets default return values.
 *
 * @param mock Mock instance (use NULL for global g_mock_rspi)
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_init(mock_rspi_t* mock);

/**
 * @brief Deinitialize the mock RSPI
 *
 * @param mock Mock instance (use NULL for global g_mock_rspi)
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_deinit(mock_rspi_t* mock);

/**
 * @brief Clear all mock state without full reinitialization
 *
 * @param mock Mock instance (use NULL for global g_mock_rspi)
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_clear(mock_rspi_t* mock);

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

/**
 * @brief Set return value for next rspi_init_peripheral call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_init_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_peripheral_transfer call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_transfer_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_peripheral_read_available call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_available_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_peripheral_write_ready call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_ready_return(mock_rspi_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next rspi_deinit call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_rspi_set_deinit_return(mock_rspi_t* mock, rx_err_t ret);

/* =============================================================================
 * Data Injection/Extraction Functions
 * =============================================================================
 */

/**
 * @brief Inject data for the next SPI receive
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 * @param data Data to inject
 * @param len Data length
 * @return k_rx_ok on success
 */
rx_err_t
mock_rspi_inject_rx_data(mock_rspi_t* mock, uint8_t channel, const uint8_t* data, uint32_t len);

/**
 * @brief Get data that was transmitted via SPI
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 * @param data Output buffer
 * @param max_len Maximum bytes to read
 * @param actual_len Actual bytes read
 * @return k_rx_ok on success
 */
rx_err_t mock_rspi_get_tx_data(mock_rspi_t* mock,
                               uint8_t      channel,
                               uint8_t*     data,
                               uint32_t     max_len,
                               uint32_t*    actual_len);

/**
 * @brief Set data availability for a channel
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 * @param available True if data is available
 */
void mock_rspi_set_data_available(mock_rspi_t* mock, uint8_t channel, bool available);

/**
 * @brief Set write ready status for a channel
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 * @param ready True if write ready
 */
void mock_rspi_set_write_ready(mock_rspi_t* mock, uint8_t channel, bool ready);

/**
 * @brief Clear a channel's buffers
 *
 * @param mock Mock instance (use NULL for global)
 * @param channel RSPI channel (0-2)
 */
void mock_rspi_clear_channel(mock_rspi_t* mock, uint8_t channel);

/* =============================================================================
 * Call Tracking Functions
 * =============================================================================
 */

/**
 * @brief Check if a function was called
 *
 * @param mock Mock instance (use NULL for global)
 * @param func Function name to check
 * @return true if function was called at least once
 */
bool mock_rspi_was_called(mock_rspi_t* mock, const char* func);

/**
 * @brief Get number of times a function was called
 *
 * @param mock Mock instance (use NULL for global)
 * @param func Function name to check
 * @return Number of calls
 */
uint32_t mock_rspi_get_call_count(mock_rspi_t* mock, const char* func);

/**
 * @brief Get the last call record for a function
 *
 * @param mock Mock instance (use NULL for global)
 * @param func Function name
 * @param out_call Output call record
 * @return k_rx_ok on success, k_rx_err_not_found if never called
 */
rx_err_t mock_rspi_get_last_call(mock_rspi_t* mock, const char* func, mock_rspi_call_t* out_call);

/**
 * @brief Clear call history
 *
 * @param mock Mock instance (use NULL for global)
 */
void mock_rspi_clear_calls(mock_rspi_t* mock);

/**
 * @brief Record a function call (internal use)
 *
 * @param mock Mock instance
 * @param func Function name
 * @param channel RSPI channel
 * @param arg1 First argument
 * @param arg2 Second argument
 * @param ret Return value
 */
void mock_rspi_record_call(mock_rspi_t* mock,
                           const char*  func,
                           uint8_t      channel,
                           uint32_t     arg1,
                           uint32_t     arg2,
                           rx_err_t     ret);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RSPI_H */
