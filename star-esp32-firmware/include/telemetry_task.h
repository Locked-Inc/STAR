/* include/telemetry_task.h - Telemetry monitoring task API */

#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Telemetry task - Monitor BMS, temperature, and motor telemetry
 *
 * Runs at 10Hz to monitor:
 * 1. Battery state (voltage, SOC, current, cell balance) via BQ7850 BMS
 * 2. System temperature via DS18B20 sensor
 * 3. Motor telemetry (setpoints, velocities, currents)
 *
 * Implements safety features:
 * - Thermal protection (emergency stop if temp > 85°C)
 * - Low battery warnings (SOC < 10%)
 * - Cell imbalance detection (>100mV difference)
 * - Battery fault reporting
 *
 * The task runs at configurable priority (default: 5 - medium)
 * and uses a dedicated 4KB stack.
 *
 * @param pvParameters System context pointer (system_context_t*)
 */
void telemetry_task(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_TASK_H */
