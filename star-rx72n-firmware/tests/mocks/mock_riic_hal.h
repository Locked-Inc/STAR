/* tests/mocks/mock_riic_hal.h */

/**
 * @file mock_riic_hal.h
 * @brief Mock RIIC (I2C) HAL Functions for Unit Testing
 *
 * Provides mock RIIC HAL functions with state tracking for testing
 * higher-level code without actual hardware.
 *
 * Features:
 * - Per-channel initialization state
 * - TX/RX data buffers
 * - Error injection (NACK, timeout, bus busy)
 * - Call history recording
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RIIC_HAL_H
#define MOCK_RIIC_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Mock RIIC constants */
typedef enum : uint16_t {
  k_mock_riic_max_channels      = 3,   /**< RIIC channels (0-2) */
  k_mock_riic_buffer_size       = 256, /**< TX/RX buffer size */
  k_mock_riic_call_history_size = 64,  /**< Call history buffer size */
} mock_riic_constants_t;

/* =============================================================================
 * Types
 * =============================================================================
 */

/** @brief RIIC HAL function call types */
typedef enum : uint8_t {
  k_mock_riic_call_init,
  k_mock_riic_call_write,
  k_mock_riic_call_read,
  k_mock_riic_call_write_read,
} mock_riic_call_type_t;

/** @brief RIIC HAL function call record */
typedef struct {
  mock_riic_call_type_t type;         /**< Call type */
  uint8_t               channel;      /**< RIIC channel */
  uint8_t               device_addr;  /**< Device address */
  uint16_t              write_length; /**< Write data length */
  uint16_t              read_length;  /**< Read data length */
} mock_riic_call_t;

/** @brief Per-channel RIIC state */
typedef struct {
  bool     initialized;                        /**< Channel initialized */
  uint32_t frequency_hz;                       /**< Configured frequency */
  uint8_t  tx_buffer[k_mock_riic_buffer_size]; /**< Last transmitted data */
  uint16_t tx_length;                          /**< Last transmit length */
  uint8_t  rx_buffer[k_mock_riic_buffer_size]; /**< Data to return on read */
  uint16_t rx_length;                          /**< Available read data length */
  uint8_t  last_device_addr;                   /**< Last device address used */
} mock_riic_channel_state_t;

/** @brief Global mock RIIC state */
typedef struct {
  mock_riic_channel_state_t channels[k_mock_riic_max_channels];          /**< Per-channel state */
  mock_riic_call_t          call_history[k_mock_riic_call_history_size]; /**< Call history */
  uint16_t                  call_count;       /**< Number of calls recorded */
  rx_err_t                  next_error;       /**< Error to return on next call */
  bool                      error_set;        /**< Whether error injection is active */
  bool                      simulate_nack;    /**< Simulate NACK response */
  bool                      simulate_timeout; /**< Simulate timeout */
  bool                      simulate_busy;    /**< Simulate bus busy */
} mock_riic_state_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/** @brief Global mock RIIC state instance */
extern mock_riic_state_t g_mock_riic;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock RIIC state
 *
 * Resets all channel states and clears call history.
 */
void mock_riic_init(void);

/**
 * @brief Reset mock RIIC state (alias for init)
 */
void mock_riic_reset(void);

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

/**
 * @brief Set data to be returned on read
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[in] data Data to return on read
 * @param[in] length Number of bytes
 */
void mock_riic_set_rx_data(uint8_t channel, const uint8_t* data, uint16_t length);

/**
 * @brief Enable NACK simulation
 *
 * @param[in] simulate True to simulate NACK
 */
void mock_riic_simulate_nack(bool simulate);

/**
 * @brief Enable timeout simulation
 *
 * @param[in] simulate True to simulate timeout
 */
void mock_riic_simulate_timeout(bool simulate);

/**
 * @brief Enable bus busy simulation
 *
 * @param[in] simulate True to simulate bus busy
 */
void mock_riic_simulate_busy(bool simulate);

/**
 * @brief Set error to return on next RIIC HAL call
 *
 * @param[in] err Error code to return
 */
void mock_riic_set_next_error(rx_err_t err);

/**
 * @brief Clear any pending error injection
 */
void mock_riic_clear_error(void);

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

/**
 * @brief Check if channel is initialized
 *
 * @param[in] channel RIIC channel (0-2)
 *
 * @return True if initialized
 */
bool mock_riic_is_initialized(uint8_t channel);

/**
 * @brief Get configured frequency for channel
 *
 * @param[in] channel RIIC channel (0-2)
 *
 * @return Frequency in Hz, or 0 if not initialized
 */
uint32_t mock_riic_get_frequency(uint8_t channel);

/**
 * @brief Get last transmitted data
 *
 * @param[in] channel RIIC channel (0-2)
 * @param[out] data Buffer to copy data to
 * @param[in] max_length Maximum bytes to copy
 *
 * @return Number of bytes copied
 */
uint16_t mock_riic_get_tx_data(uint8_t channel, uint8_t* data, uint16_t max_length);

/**
 * @brief Get last device address used
 *
 * @param[in] channel RIIC channel (0-2)
 *
 * @return Device address
 */
uint8_t mock_riic_get_last_device_addr(uint8_t channel);

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

/**
 * @brief Get call history entry
 *
 * @param[in] index Index in call history
 *
 * @return Pointer to call record, or NULL if index out of range
 */
const mock_riic_call_t* mock_riic_get_call(uint16_t index);

/**
 * @brief Get total number of recorded calls
 *
 * @return Number of calls in history
 */
uint16_t mock_riic_get_call_count(void);

/**
 * @brief Clear call history
 */
void mock_riic_clear_history(void);

/* =============================================================================
 * RIIC HAL Function Declarations (for test linking)
 * =============================================================================
 */

rx_err_t riic_init(uint8_t channel, uint32_t frequency_hz);
rx_err_t
riic_write(uint8_t channel, const uint8_t device_addr, const uint8_t* data, const uint16_t length);
rx_err_t
riic_read(uint8_t channel, const uint8_t device_addr, uint8_t* data, const uint16_t length);
rx_err_t riic_write_read(uint8_t        channel,
                         uint8_t        device_addr,
                         const uint8_t* write_data,
                         uint16_t       write_length,
                         uint8_t*       read_data,
                         uint16_t       read_length);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RIIC_HAL_H */
