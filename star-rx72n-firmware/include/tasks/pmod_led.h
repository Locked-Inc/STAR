/* include/tasks/pmod_led.h */

/**
 * @file pmod_led.h
 * @brief PMOD LED Status Display Task
 * @details
 * Displays system status on Pmod8LD (8 LEDs, GPIO output).
 * Updates at 10 Hz to show real-time system health.
 *
 * LED Assignments:
 * - LED 0-3: Motor fault indicators (one per motor)
 * - LED 4: Emergency stop active
 * - LED 5: Communication timeout
 * - LED 6: Obstacle detected
 * - LED 7: Battery low warning
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_PMOD_LED_H
#define STAR_RX72N_PMOD_LED_H

#include <stdint.h>

#include "tx_api.h"

/**
 * @brief Create PMOD LED status display task
 *
 * Creates ThreadX thread for LED status display at 10 Hz.
 * Call from tx_application_define() after core threads.
 *
 * @return TX_SUCCESS on success, error code otherwise
 */
UINT pmod_led_create(void);

#endif /* STAR_RX72N_PMOD_LED_H */
