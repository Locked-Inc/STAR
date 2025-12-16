/* include/motor_control_task.h - Motor control task API */

#ifndef MOTOR_CONTROL_TASK_H
#define MOTOR_CONTROL_TASK_H

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Motor control task - Sequential control of 4 motors
 *
 * Processes all 4 motors sequentially within a 4ms cycle (250Hz update rate).
 * Each cycle performs the following for all 4 motors:
 * 1. Select encoder via multiplexer
 * 2. Read position and calculate velocity
 * 3. Compute PID output
 * 4. Apply PWM to motor
 * 5. Read motor current (ADC)
 * 6. Check for overcurrent protection
 *
 * The task runs at configurable priority (default: 10 - highest)
 * and uses a dedicated 8KB stack.
 *
 * @param pvParameters System context pointer (system_context_t*)
 */
void motor_control_task(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CONTROL_TASK_H */
