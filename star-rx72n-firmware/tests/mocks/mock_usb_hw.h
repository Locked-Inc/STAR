/* tests/mocks/mock_usb_hw.h */

/**
 * @file mock_usb_hw.h
 * @brief Mock USB Hardware Layer for Host-Side Testing
 *
 * Mock implementation of the USB hardware layer (rx_usb_hw.c) for testing.
 * Provides call tracking, error injection, and data injection capabilities.
 *
 * This follows the same pattern as mock_error_handler.h from rx_core.
 *
 * Features:
 * - Records all hardware function calls with parameters
 * - Allows configuration of return values for error testing
 * - Provides FIFO data injection/extraction for data path testing
 * - No actual hardware access (runs on host)
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_USB_HW_H
#define MOCK_USB_HW_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_usb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/** @brief Maximum number of function calls to track */
#define MOCK_USB_HW_CALL_HISTORY_SIZE 64

/** @brief Maximum FIFO buffer size per pipe */
#define MOCK_USB_HW_FIFO_SIZE 512

/** @brief Maximum number of pipes (0=DCP, 1-9=data pipes) */
#define MOCK_USB_HW_MAX_PIPES 10

/** @brief Maximum function name length */
#define MOCK_USB_HW_FUNC_NAME_MAX 32

/* =============================================================================
 * Call Record Structure
 * =============================================================================
 */

/**
 * @brief Record of a single function call
 */
typedef struct {
  char     function[MOCK_USB_HW_FUNC_NAME_MAX]; /**< Function name */
  uint32_t arg1;                                /**< First argument (context-dependent) */
  uint32_t arg2;                                /**< Second argument */
  uint32_t arg3;                                /**< Third argument */
  rx_err_t return_value;                        /**< Return value */
} mock_usb_hw_call_t;

/* =============================================================================
 * Pipe State Structure
 * =============================================================================
 */

/**
 * @brief State of a single USB pipe
 */
typedef struct {
  bool     configured;                       /**< Pipe is configured */
  uint8_t  endpoint;                         /**< Endpoint number */
  bool     is_in;                            /**< IN direction (device to host) */
  uint16_t type;                             /**< Pipe type (bulk, interrupt, etc.) */
  uint16_t max_packet;                       /**< Maximum packet size */
  uint8_t  rx_data[MOCK_USB_HW_FIFO_SIZE];   /**< RX FIFO data (host to device) */
  uint32_t rx_len;                           /**< Bytes in RX FIFO */
  uint32_t rx_pos;                           /**< Read position in RX FIFO */
  uint8_t  tx_data[MOCK_USB_HW_FIFO_SIZE];   /**< TX FIFO data (device to host) */
  uint32_t tx_len;                           /**< Bytes in TX FIFO */
} mock_pipe_state_t;

/* =============================================================================
 * Mock USB Hardware State
 * =============================================================================
 */

/**
 * @brief Mock USB hardware state structure
 */
typedef struct {
  /* Hardware state */
  bool           initialized;                     /**< Hardware initialized */
  bool           attached;                        /**< D+ pull-up enabled */
  uint8_t        address;                         /**< USB device address */
  rx_usb_state_t bus_state;                       /**< Current bus state */

  /* Pipe states */
  mock_pipe_state_t pipes[MOCK_USB_HW_MAX_PIPES]; /**< Pipe state array */

  /* Call tracking */
  mock_usb_hw_call_t call_history[MOCK_USB_HW_CALL_HISTORY_SIZE]; /**< Call history */
  uint32_t           call_count;                                  /**< Total calls recorded */
  uint32_t           call_write_index;                            /**< Next write index (circular) */

  /* Configurable return values for error injection */
  rx_err_t next_init_return;   /**< Return value for next hw_init call */
  rx_err_t next_attach_return; /**< Return value for next hw_attach call */
  bool     fifo_ready;         /**< FIFO ready flag (affects reads/writes) */
  uint32_t fifo_timeout_after; /**< Simulate timeout after N accesses */

  /* Statistics */
  uint32_t init_calls;       /**< Number of hw_init calls */
  uint32_t deinit_calls;     /**< Number of hw_deinit calls */
  uint32_t attach_calls;     /**< Number of hw_attach calls */
  uint32_t detach_calls;     /**< Number of hw_detach calls */
  uint32_t fifo_read_calls;  /**< Number of fifo_read calls */
  uint32_t fifo_write_calls; /**< Number of fifo_write calls */
} mock_usb_hw_t;

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

extern mock_usb_hw_t g_mock_usb_hw;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize the mock USB hardware
 *
 * Clears all state and sets default return values.
 *
 * @param mock Mock instance (use NULL for global g_mock_usb_hw)
 * @return k_rx_ok on success
 */
rx_err_t mock_usb_hw_init(mock_usb_hw_t* mock);

/**
 * @brief Deinitialize the mock USB hardware
 *
 * @param mock Mock instance (use NULL for global g_mock_usb_hw)
 * @return k_rx_ok on success
 */
rx_err_t mock_usb_hw_deinit(mock_usb_hw_t* mock);

/**
 * @brief Clear all mock state without full reinitialization
 *
 * @param mock Mock instance (use NULL for global g_mock_usb_hw)
 * @return k_rx_ok on success
 */
rx_err_t mock_usb_hw_clear(mock_usb_hw_t* mock);

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

/**
 * @brief Set return value for next hw_init call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_usb_hw_set_init_return(mock_usb_hw_t* mock, rx_err_t ret);

/**
 * @brief Set return value for next hw_attach call
 *
 * @param mock Mock instance (use NULL for global)
 * @param ret Return value to use
 */
void mock_usb_hw_set_attach_return(mock_usb_hw_t* mock, rx_err_t ret);

/**
 * @brief Set FIFO ready state
 *
 * @param mock Mock instance (use NULL for global)
 * @param ready True if FIFO is ready for access
 */
void mock_usb_hw_set_fifo_ready(mock_usb_hw_t* mock, bool ready);

/**
 * @brief Set bus state
 *
 * @param mock Mock instance (use NULL for global)
 * @param state New bus state
 */
void mock_usb_hw_set_bus_state(mock_usb_hw_t* mock, rx_usb_state_t state);

/* =============================================================================
 * Data Injection/Extraction Functions
 * =============================================================================
 */

/**
 * @brief Inject data into a pipe's RX FIFO (simulates host sending data)
 *
 * @param mock Mock instance (use NULL for global)
 * @param pipe Pipe number (0-9)
 * @param data Data to inject
 * @param len Data length
 * @return k_rx_ok on success, k_rx_err_invalid_arg if pipe invalid
 */
rx_err_t mock_usb_hw_inject_rx_data(mock_usb_hw_t* mock,
                                    uint8_t        pipe,
                                    const uint8_t* data,
                                    uint32_t       len);

/**
 * @brief Get data from a pipe's TX FIFO (simulates host reading data)
 *
 * @param mock Mock instance (use NULL for global)
 * @param pipe Pipe number (0-9)
 * @param data Output buffer
 * @param max_len Maximum bytes to read
 * @param actual_len Actual bytes read
 * @return k_rx_ok on success
 */
rx_err_t mock_usb_hw_get_tx_data(mock_usb_hw_t* mock,
                                 uint8_t        pipe,
                                 uint8_t*       data,
                                 uint32_t       max_len,
                                 uint32_t*      actual_len);

/**
 * @brief Clear a pipe's FIFO buffers
 *
 * @param mock Mock instance (use NULL for global)
 * @param pipe Pipe number (0-9)
 */
void mock_usb_hw_clear_pipe(mock_usb_hw_t* mock, uint8_t pipe);

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
bool mock_usb_hw_was_called(mock_usb_hw_t* mock, const char* func);

/**
 * @brief Get number of times a function was called
 *
 * @param mock Mock instance (use NULL for global)
 * @param func Function name to check
 * @return Number of calls
 */
uint32_t mock_usb_hw_get_call_count(mock_usb_hw_t* mock, const char* func);

/**
 * @brief Get the last call record for a function
 *
 * @param mock Mock instance (use NULL for global)
 * @param func Function name
 * @param out_call Output call record
 * @return k_rx_ok on success, k_rx_err_not_found if function never called
 */
rx_err_t mock_usb_hw_get_last_call(mock_usb_hw_t*      mock,
                                   const char*         func,
                                   mock_usb_hw_call_t* out_call);

/**
 * @brief Clear call history
 *
 * @param mock Mock instance (use NULL for global)
 */
void mock_usb_hw_clear_calls(mock_usb_hw_t* mock);

/* =============================================================================
 * Internal: Record a call (used by mock implementations)
 * =============================================================================
 */

/**
 * @brief Record a function call (internal use)
 *
 * @param mock Mock instance
 * @param func Function name
 * @param arg1 First argument
 * @param arg2 Second argument
 * @param arg3 Third argument
 * @param ret Return value
 */
void mock_usb_hw_record_call(mock_usb_hw_t* mock,
                             const char*    func,
                             uint32_t       arg1,
                             uint32_t       arg2,
                             uint32_t       arg3,
                             rx_err_t       ret);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_USB_HW_H */
