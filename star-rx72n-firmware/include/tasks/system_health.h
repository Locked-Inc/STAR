/* star-rx72n-firmware/include/tasks/system_health.h */

/**
 * @file system_health.h
 * @brief System Health thread for battery and diagnostics monitoring
 * @details
 * Low priority thread (priority 9) running at 1 Hz.
 * Monitors battery (BQ4050), diagnostics, and register guard.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include "tx_api.h"

/**
 * @brief Create and start System_Health thread
 * @return TX_SUCCESS on success, error code on failure
 */
UINT system_health_create(void);

#endif /* SYSTEM_HEALTH_H */
