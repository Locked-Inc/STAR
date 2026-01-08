/* include/tasks/comm_manager.h */

/**
 * @file comm_manager.h
 * @brief Communication Manager thread for RPi5 communication
 * @details
 * High priority thread (priority 3) running at 100 Hz for USB/SPI
 * communication with Raspberry Pi 5.
 *
 * Thread Responsibilities:
 * - Receive velocity commands from RPi5 (USB CDC primary, SPI secondary)
 * - Send encoder telemetry to RPi5
 * - Protocol Buffer messaging with nanopb
 * - Communication timeout watchdog (500ms → E-STOP)
 * - Manual emergency stop clearance
 *
 * Priority Rationale:
 * High priority (3) because RPi5 is "High-Level Commander".
 * Missed commands make robot feel laggy.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef COMM_MANAGER_H
#define COMM_MANAGER_H

#include "tx_api.h"

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Create and start Comm_Manager thread
 * @return TX_SUCCESS on success, error code on failure
 */
UINT comm_manager_create(void);

#endif /* COMM_MANAGER_H */
