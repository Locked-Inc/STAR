/**
 * @file comm_task.h
 * @brief Communication Task - Multi-Channel Protocol Handler for Raspberry Pi 5
 *
 * @details
 * Declares the communication task and the unified system-configuration API used
 * to select which comm channels carry binary frames and which interfaces carry
 * ASCII debug logs at runtime.
 *
 * **Key concern - SCI9 conflict:**
 * Both uart_debug_puts() (ASCII log backend) and rx_uart_comm_send() (binary
 * nanopb frames) write to SCI9.  comm_task_apply_system_config() detects this
 * combination at config time and returns k_rx_err_invalid_arg, preventing a
 * live hardware conflict.
 *
 * @see comm_task.c Implementation
 * @see rx_log.h     Log backend selection API
 * @see rx_comm_manager.h  Channel mask definitions
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "rx_comm_manager.h"
#include "rx_err.h"
#include "rx_log.h"

/* =============================================================================
 * System Configuration
 * =============================================================================
 */

/**
 * @struct rx_system_config_t
 * @brief Unified runtime configuration for comm channels and log backends
 *
 * @details
 * Groups the two orthogonal runtime bitmasks so they can be validated together
 * by comm_task_apply_system_config() before either is applied.  This single
 * validation point is the only place that can detect the SCI9 conflict between
 * UART comm (binary frames on SCI9) and UART log (ASCII text on SCI9).
 *
 * @par Example -- USB+SPI+I2C comm, logs on both interfaces:
 * @code
 * rx_system_config_t cfg = {
 *     .comm_channels = (rx_comm_channel_mask_t)(
 *         k_comm_channel_mask_usb | k_comm_channel_mask_spi | k_comm_channel_mask_i2c),
 *     .log_backends  = k_log_backend_both,
 * };
 * comm_task_apply_system_config(&cfg);
 * @endcode
 *
 * @invariant (comm_channels & k_comm_channel_mask_uart) and (log_backends & k_log_backend_uart)
 *   must not both be set -- SCI9 cannot simultaneously carry binary frames (UART comm)
 *   and ASCII debug text (UART log).  comm_task_apply_system_config() enforces this
 *   and returns k_rx_err_invalid_arg when the constraint is violated.
 *
 * @see comm_task_apply_system_config() Validation and apply function
 * @see k_system_config_default        Safe startup default
 * @since Version 1.0.0
 */
typedef struct {
  rx_comm_channel_mask_t comm_channels; /**< Channels that carry binary nanopb frames */
  rx_log_backend_t       log_backends;  /**< Interfaces that carry ASCII debug logs */
} rx_system_config_t;

/**
 * @brief Safe default: USB+SPI+I2C comm (no UART comm), logs on both interfaces
 *
 * @details
 * Excludes UART comm (k_comm_channel_mask_uart) so that SCI9 remains free for
 * ASCII log output on both UART and USB CDC Port 2 simultaneously.  Suitable
 * for all production configurations that do not require binary UART framing.
 *
 * @since Version 1.0.0
 */
extern const rx_system_config_t k_system_config_default;

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Validate and apply a system configuration atomically
 *
 * @details
 * Performs three checks before modifying any state:
 * 1. Null pointer guard on config
 * 2. Unknown bits in comm_channels or log_backends → k_rx_err_invalid_arg
 * 3. SCI9 conflict: UART comm AND UART log both set → k_rx_err_invalid_arg
 *
 * If all checks pass, calls rx_log_set_backend() and stores comm_channels
 * in the file-scoped s_enabled_channels variable used by
 * internal_init_transports().
 *
 * @param[in] config  Pointer to the configuration to apply (must not be NULL)
 *
 * @return rx_err_t
 * @retval k_rx_ok              Configuration applied
 * @retval k_rx_err_null_ptr    config is NULL
 * @retval k_rx_err_invalid_arg Unknown bits, or SCI9 conflict detected
 *
 * @pre config != NULL
 * @pre config->log_backends only contains bits from rx_log_backend_t
 * @post rx_log_get_backend() reflects config->log_backends on success
 * @post s_enabled_channels reflects config->comm_channels on success
 * @post No state changed on error return
 *
 * @note Not thread-safe. Call before comm_task_create().
 * @see rx_log_set_backend() Backend setter called internally
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [PASS] 3 preconditions, 2 postconditions
 */
rx_err_t comm_task_apply_system_config(const rx_system_config_t* config);

/**
 * @brief Create and start the communication task
 *
 * @details
 * Creates the CommTask ThreadX task for multi-channel protocol handling.
 * Call comm_task_apply_system_config() before this function to configure
 * which channels to enable.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok              Task created successfully
 * @retval k_rx_err_invalid_state Task already created
 * @retval k_rx_err_rtos_thread_create ThreadX thread creation failed
 *
 * @pre shared_data_init() called
 * @pre comm_task_apply_system_config() called (or defaults are acceptable)
 *
 * @post CommTask scheduled and will initialize transports on first run
 *
 * @note Call from tx_application_define() after shared_data_init()
 * @see comm_task_apply_system_config() Configure before creating
 * @see k_system_config_default Safe default configuration
 * @since Version 1.0.0
 */
rx_err_t comm_task_create(void);
