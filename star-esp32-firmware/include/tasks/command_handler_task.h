/**
 * @file command_handler_task.h
 * @brief Public API for the command handler FreeRTOS task
 * @details
 * This header defines the public interface for the command handler task that receives
 * motor control commands from the Raspberry Pi 5 over SPI using ARQ Protocol and
 * Protocol Buffers. The task processes SetVelocityRequest, EmergencyStopRequest, and
 * other motor control messages, updating the shared state for the motor control task.
 *
 * Key Features:
 * - Receives and processes Protocol Buffer messages via ARQ/SPI
 * - Updates shared state with velocity commands
 * - Handles emergency stop requests
 * - Sends motor status responses back to RPi5
 * - Skid-steer kinematics: left/right velocity distribution
 *
 * Example Usage:
 * @code
 * // Initialize ARQ and shared state (in main.c)
 * star_arq_handle_t arq;
 * motor_shared_state_t shared_state;
 * // ... initialize components ...
 *
 * // Create and start command handler task
 * command_handler_task_create(&shared_state, &arq);
 * @endcode
 *
 * @date 2025-12-20
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef COMMAND_HANDLER_TASK_H
#define COMMAND_HANDLER_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "motor_shared_state.h"
#include "star_arq.h"

/* --- Configuration Constants --- */

/**
 * @brief Protocol Buffer buffer configuration
 */
typedef enum {
  k_command_rx_buffer_size = 512,  /**< Request receive buffer (bytes) */
  k_command_tx_buffer_size = 1024, /**< Response transmit buffer (bytes) */
} command_buffer_config_t;

/**
 * @brief ARQ configuration
 */
typedef enum {
  k_command_arq_error_backoff_ms = 100,  /**< Delay after receive error (ms) */
  k_command_arq_send_timeout_ms  = 1000, /**< Timeout for ARQ send (ms) */
} command_arq_config_t;

/**
 * @brief Task configuration
 */
typedef enum {
  k_command_task_stack_size = 6144, /**< Stack size (bytes) */
  k_command_task_priority   = 6,    /**< Task priority (lower than motor control) */
} command_task_config_t;

/* --- Public Function Prototypes --- */

/**
 * @brief Create and start the command handler task
 *
 * This task handles:
 * - Receiving Protocol Buffer messages via ARQ/SPI
 * - Processing SetVelocityRequest (distributes left/right to motors 0-3)
 * - Processing EmergencyStopRequest
 * - Sending motor status responses
 *
 * @param[in] shared_state Pointer to initialized shared state structure
 * @param[in] arq          Pointer to initialized ARQ handle
 *
 * @note Both parameters must remain valid for the lifetime of the task
 */
void command_handler_task_create(motor_shared_state_t* shared_state, star_arq_handle_t* arq);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_HANDLER_TASK_H */
