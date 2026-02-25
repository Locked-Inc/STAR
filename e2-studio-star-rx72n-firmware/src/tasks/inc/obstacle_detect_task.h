/**
 * @file obstacle_detect_task.h
 * @brief Obstacle Detection Task - HC-SR04 Ultrasonic Sensor Monitoring
 *
 * @details
 * Declares the obstacle detection task responsible for monitoring ultrasonic
 * sensors to detect obstacles and trigger emergency stops if needed.
 *
 * **Responsibilities:**
 * - Trigger and read HC-SR04 ultrasonic sensors
 * - Calculate distance to obstacles
 * - Monitor critical zones (e.g., < 10 cm)
 * - Trigger emergency stop on imminent collision
 * - Update shared obstacle data for path planning
 *
 * **Sensor Configuration:**
 * - Sensors: 4x HC-SR04 ultrasonic rangefinders
 * - Range: 2 cm to 400 cm
 * - Trigger: GPTW timer for pulse generation
 * - Echo: MTU input capture for timing
 * - Update rate: 50 Hz (20 ms period)
 *
 * @see obstacle_detect_task.c Implementation
 * @see rx_hcsr04.h HC-SR04 driver
 * @see shared_data.h Obstacle distance data
 *
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the obstacle detection task
 *
 * @details
 * Creates the ObstacleTask ThreadX task for ultrasonic sensor monitoring.
 * This task periodically triggers sensors and detects obstacles.
 *
 * **Task Configuration:**
 * - Priority: 12 (medium-low priority - safety-critical but not time-critical)
 * - Stack: 2048 bytes
 * - Period: 20 ms (50 Hz obstacle detection)
 * - Auto-start: Enabled
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_* ThreadX task creation failed
 *
 * @pre ThreadX kernel running
 * @pre HC-SR04 sensors initialized
 * @pre GPTW/MTU timers configured
 * @pre Shared data initialized
 *
 * @post ObstacleTask created and running at 50 Hz
 * @post Obstacle distances updated in shared memory
 * @post Emergency stop triggered on close obstacles
 *
 * @note Call from tx_application_define() in main.c after sensor initialization
 * @note Safety-critical - obstacles closer than 10 cm trigger emergency stop
 *
 * @see obstacle_detect_task.c Implementation details
 * @see main.c Task creation in tx_application_define()
 *
 * @since STAR v1.0.0
 */
rx_err_t obstacle_detect_task_create(void);
