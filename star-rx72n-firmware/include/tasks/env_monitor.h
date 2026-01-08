/* include/tasks/env_monitor.h */

/**
 * @file env_monitor.h
 * @brief Environment Monitor thread for obstacle detection and temperature
 * @details
 * Medium priority thread (priority 5) running at 50 Hz.
 * Monitors HC-SR04 ultrasonic sensors and DS18B20 temperature sensor.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef ENV_MONITOR_H
#define ENV_MONITOR_H

#include "tx_api.h"

/**
 * @brief Create and start Env_Monitor thread
 * @return TX_SUCCESS on success, error code on failure
 */
UINT env_monitor_create(void);

#endif /* ENV_MONITOR_H */
