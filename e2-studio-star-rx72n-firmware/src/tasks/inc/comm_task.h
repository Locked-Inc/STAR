/**
 * @file comm_task.h
 * @brief Communication Task - SPI Protocol Handler for Raspberry Pi 5
 *
 * @details
 * Declares the communication task responsible for handling Protocol Buffers
 * communication over SPI with the Raspberry Pi 5 main controller.
 *
 * **Responsibilities:**
 * - Receive command frames from RPi5 via SPI
 * - Parse and validate Protocol Buffer messages (nanopb)
 * - Dispatch commands to appropriate handlers (motor control, telemetry requests)
 * - Send response frames with telemetry data
 * - Handle communication errors and timeouts
 *
 * **Communication Protocol:**
 * - Physical: SPI peripheral (RX72N) <- SPI controller (RPi5)
 * - Data link: Custom frame protocol with CRC-32
 * - Application: Protocol Buffers (nanopb) messages
 * - Commands: MotorCommand, EmergencyStop, TelemetryRequest
 * - Responses: MotorStatus, BatteryStatus, SensorData
 *
 * @see comm_task.c Implementation
 * @see rx_nanopb.h Protocol Buffers encoder/decoder
 * @see rx_frame.h Frame protocol with CRC
 *
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the communication task
 *
 * @details
 * Creates the CommTask ThreadX task for SPI protocol handling. This task
 * receives commands from the Raspberry Pi 5 and dispatches them to the
 * appropriate system components.
 *
 * **Task Configuration:**
 * - Priority: 8 (high priority - responsive to RPi5 commands)
 * - Stack: 3072 bytes (Protocol Buffers decoding requires stack space)
 * - Trigger: Event-driven (SPI RX interrupt)
 * - Auto-start: Enabled
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_* ThreadX task creation failed
 *
 * @pre ThreadX kernel running
 * @pre SPI peripheral initialized
 * @pre Protocol Buffers decoder initialized
 * @pre Shared data initialized
 *
 * @post CommTask created and waiting for SPI frames
 * @post Commands from RPi5 will be processed
 *
 * @note Call this from AppMainTask after SPI initialization
 * @note Task blocks on event flag - woken by SPI RX interrupt
 *
 * @see comm_task.c Implementation details
 * @see app_main_task.c Task creation coordinator
 *
 * @since STAR v1.0.0
 */
rx_err_t comm_task_create(void);
