/**
 * @file spi_comm_task.h
 * @brief Public API for the SPI Communication ThreadX task
 *
 * This task handles Protocol Buffer communication with the RPi5 controller
 * over SPI. It receives commands (velocity, e-stop), dispatches them to
 * appropriate handlers, and sends telemetry/status responses.
 *
 * Key Features:
 * - 100Hz polling rate for SPI commands
 * - Protocol Buffer encoding/decoding via nanopb
 * - Frame layer with CRC-32 validation
 * - Optional FEC/HARQ for reliability
 * - Integration with motor control via shared state
 *
 * Communication Flow:
 * 1. RPi5 sends command frame → RX72N decodes → dispatches to handler
 * 2. Handler updates motor setpoints via shared state
 * 3. RX72N encodes response → sends back to RPi5
 *
 * @see rx_spi_comm.h for SPI communication layer
 * @see rx_nanopb.h for Protocol Buffer encoding/decoding
 * @see rx_frame.h for frame layer
 * @see docs/sections/01_nanopb_protocol.tex for protocol specification
 *
 * @date 2025-12-27
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef SPI_COMM_TASK_H
#define SPI_COMM_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "motor_shared_state.h"
#include "rx_err.h"
#include "tx_api.h"

/* =============================================================================
 * Configuration Constants
 * =============================================================================
 */

/**
 * @brief SPI communication task configuration
 */
typedef enum {
    k_spi_comm_task_poll_hz     = 100, /**< SPI polling frequency (Hz) */
    k_spi_comm_task_period_ms   = 10,  /**< Polling period (ms) - 1000/100 */
    k_spi_comm_task_stack_size  = 4096, /**< Stack size (bytes) */
    k_spi_comm_task_priority    = 8,    /**< Task priority (lower than motor) */
    k_spi_comm_rspi_channel     = 0,    /**< RSPI channel for RPi5 */
    k_spi_comm_spi_mode         = 0,    /**< SPI mode 0 (CPOL=0, CPHA=0) */
} spi_comm_task_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Create and start the SPI communication task
 *
 * Initializes SPI hardware, frame encoder/decoder, and nanopb wrapper,
 * then starts the task that handles RPi5 communication.
 *
 * This task handles:
 * - Receiving command frames from RPi5
 * - Decoding Protocol Buffer messages
 * - Dispatching velocity commands to motor control (via shared state)
 * - Handling emergency stop commands
 * - Sending telemetry and status responses
 * - ACK/NACK protocol management
 *
 * @param[in] shared_state Pointer to initialized motor shared state
 *
 * @return RX_OK on success
 * @return RX_ERR_INVALID_ARG if shared_state is NULL
 * @return RX_ERR_INVALID_STATE if task creation fails
 *
 * @note The shared state must be initialized before calling this function
 * @note SPI pins must be configured (via MPC) before task creation
 */
rx_err_t spi_comm_task_create(motor_shared_state_t *shared_state);

/**
 * @brief Get SPI communication statistics
 *
 * Returns statistics about frames sent/received, errors, etc.
 *
 * @param[out] frames_rx Number of frames received
 * @param[out] frames_tx Number of frames transmitted
 * @param[out] crc_errors Number of CRC validation failures
 * @param[out] decode_errors Number of Protocol Buffer decode failures
 *
 * @return RX_OK on success
 */
rx_err_t spi_comm_task_get_stats(uint32_t *frames_rx, uint32_t *frames_tx,
                                 uint32_t *crc_errors, uint32_t *decode_errors);

/**
 * @brief Reset SPI communication statistics
 *
 * Clears all counters to zero.
 */
void spi_comm_task_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* SPI_COMM_TASK_H */
