/**
 * @file telemetry_task.h
 * @brief Public API for the telemetry FreeRTOS task
 * @details
 * This header defines the public interface for the telemetry task that handles
 * temperature sensor readings from the DS18B20 sensor and communicates with the
 * Raspberry Pi 5 controller over SPI using Protocol Buffers with ARQ reliability.
 *
 * Key Features:
 * - Receives GetTelemetryRequest messages via ARQ/SPI peripheral interface
 * - Reads DS18B20 temperature sensor data over 1-Wire
 * - Sends GetTelemetryResponse messages with temperature data encoded using nanopb
 * - Configurable buffer sizes and timeout parameters via enum constants
 *
 * Example Usage:
 * @code
 * // Initialize hardware components
 * star_ds18b20_handle_t temp_sensor;
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "star_arq.h"
#include "star_sensor_ds18b20.h"

/**
 * star_arq_handle_t arq;
 * // ... initialize sensor and ARQ ...
 *
 * // Create and start telemetry task
 * telemetry_task_create(&temp_sensor, &arq);
 * @endcode
 */

/* --- Configuration Constants --- */

/* Protocol Buffer configuration */
typedef enum {
  k_rx_buffer_size = 256, /* Request receive buffer (bytes) */
  k_tx_buffer_size = 512, /* Response transmit buffer (bytes) */
} protocol_buffer_config_t;

/* ARQ configuration */
typedef enum {
  k_arq_error_backoff_ms = 100,  /* Delay after receive error (ms) */
  k_arq_send_timeout_ms  = 1000, /* Timeout for ARQ send (ms) */
} telemetry_arq_config_t;

/* Task configuration */
typedef enum {
  k_telemetry_task_stack_size = 4096, /* Stack size (bytes) */
  k_telemetry_task_priority   = 5,    /* Task priority */
} task_config_t;

/**
 * @brief Create and start the telemetry task
 *
 * This task handles:
 * - Receiving GetTelemetryRequest messages via ARQ/SPI
 * - Reading DS18B20 temperature sensor
 * - Sending GetTelemetryResponse messages
 *
 * @param sensor Pointer to initialized DS18B20 sensor handle
 * @param arq Pointer to initialized ARQ handle
 *
 * @note sensor and arq must remain valid for the lifetime of the task
 */
void telemetry_task_create(star_ds18b20_handle_t* sensor, star_arq_handle_t* arq);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_TASK_H */
