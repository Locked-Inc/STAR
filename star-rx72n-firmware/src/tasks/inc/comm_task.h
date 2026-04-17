/**
 * @file comm_task.h
 * @brief Communication Task -- Multi-Channel Protocol Handler for Raspberry Pi 5
 *
 * @details
 * Declares the communication task and the unified system-configuration API
 * used to select which comm channels carry binary protobuf frames at runtime.
 *
 * Supported channels: SPI (RSPI2 to ROS2 spi_driver_node), I2C (RIIC0), and
 * UART (SCI9 via CY7C65213 USB-UART bridge to the gateway). The UART channel
 * also carries log output by serializing every log line as a
 * @ref k_frame_type_log_message frame -- see rx_log_uart.h.
 *
 * There is no longer a log-backend runtime selector or an SCI9 conflict gate:
 * logs always flow through the framed UART path, which is compatible with
 * protobuf command / response / telemetry traffic on the same byte stream.
 *
 * @see comm_task.c           Implementation
 * @see rx_log_uart.h         Log ring buffer + drain API
 * @see rx_comm_manager.h     Channel mask definitions
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "rx_comm_manager.h"
#include "rx_err.h"

/* =============================================================================
 * System Configuration
 * =============================================================================
 */

/**
 * @struct rx_system_config_t
 * @brief Unified runtime configuration for comm channels
 *
 * @details
 * Selects which communication channels the comm task initializes. Each bit in
 * @ref comm_channels corresponds to one @ref rx_comm_channel_t value.
 *
 * @par Example -- UART + SPI + I2C:
 * @code
 * const rx_system_config_t cfg = {
 *     .comm_channels = (rx_comm_channel_mask_t)(
 *         k_comm_channel_mask_uart | k_comm_channel_mask_spi | k_comm_channel_mask_i2c),
 * };
 * rx_err_t err = comm_task_apply_system_config(&cfg);
 * @endcode
 *
 * @see comm_task_apply_system_config() Validation and apply function
 * @see k_system_config_default         Safe startup default
 * @since Version 1.0.0
 */
typedef struct {
  rx_comm_channel_mask_t comm_channels; /**< Channels that carry binary frames */
} rx_system_config_t;

/**
 * @brief Safe default: UART + SPI + I2C (UART is the sole link to the gateway)
 *
 * @since Version 1.0.0
 */
extern const rx_system_config_t k_system_config_default;

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Validate and apply a system configuration
 *
 * @param[in] config System configuration to apply (must not be NULL)
 *
 * @retval k_rx_ok              Configuration applied
 * @retval k_rx_err_null_ptr    config is NULL
 * @retval k_rx_err_invalid_arg Unknown bits in comm_channels
 *
 * @pre config != NULL
 * @pre Must be called before comm_task_create()
 * @post s_enabled_channels reflects comm_channels on k_rx_ok
 *
 * @note Not thread-safe. Call once at startup.
 * @since Version 1.0.0
 */
rx_err_t comm_task_apply_system_config(const rx_system_config_t* config);

/**
 * @brief Create and start the communication task
 *
 * @retval k_rx_ok                    Task created successfully
 * @retval k_rx_err_invalid_state     Task already created
 * @retval k_rx_err_rtos_thread_create ThreadX thread creation failed
 *
 * @pre shared_data_init() called
 * @pre comm_task_apply_system_config() called (or defaults are acceptable)
 * @post CommTask scheduled; transports initialized on first run
 *
 * @since Version 1.0.0
 */
rx_err_t comm_task_create(void);
