/* include/communication_task.h - Communication task API */

#ifndef COMMUNICATION_TASK_H
#define COMMUNICATION_TASK_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Communication task - SPI peripheral communication with RPi5
 *
 * Handles bidirectional SPI communication with Raspberry Pi 5 at 100Hz:
 *
 * Commands received from RPi5:
 * - 0x01: Set motor speed (motor ID + setpoint)
 * - 0x02: Stop motor (motor ID)
 * - 0x03: Emergency stop (all motors)
 * - 0x10: Request telemetry
 *
 * Telemetry sent to RPi5:
 * - Status byte
 * - 4x motor RPM (int16_t)
 * - 4x motor current (int16_t, mA)
 *
 * Protocol features:
 * - CRC-8 error detection on received commands
 * - 5-byte command packets
 * - 17-byte telemetry response packets
 *
 * The task runs at configurable priority (default: 7 - high)
 * and uses a dedicated 4KB stack.
 *
 * @param pvParameters System context pointer (system_context_t*)
 */
void communication_task(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* COMMUNICATION_TASK_H */
