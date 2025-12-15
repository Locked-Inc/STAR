/* include/system_context.h - System context structure */

#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>

#include "star_bms_bq7850.h"
#include "star_bus_manager.h"
#include "star_encoder.h"
#include "star_error_handler.h"
#include "star_motor.h"
#include "star_pid.h"
/* TODO: Re-enable once DS18B20 and 1-Wire are implemented */
/* #include "star_sensor_ds18b20.h" */

#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System context holding all hardware handles and state
 *
 * This structure maintains the complete system state including all hardware
 * drivers, sensor interfaces, task handles, and shared state variables.
 * It follows the Dependency Inversion Principle by holding interface pointers
 * and injecting them into dependent components.
 */
typedef struct {
    /* ===== Infrastructure ===== */
    error_handler_t        error_handler; /**< Error handler with retry logic */
    star_error_interface_t error_iface;   /**< Error handling interface */
    star_pin_interface_t   pin_iface;     /**< Pin validation interface */
    star_bus_manager_t     bus_manager;   /**< Unified bus abstraction */

    /* ===== Motor Drivers ===== */
    star_motor_handle_t motors[NUM_MOTORS]; /**< Low-level MCPWM motor controllers */

    /* ===== Encoder (Multiplexed) ===== */
    star_encoder_handle_t encoder; /**< Single shared encoder (multiplexed across 4 motors) */

    /* ===== PID Controllers ===== */
    star_pid_handle_t pid[NUM_MOTORS]; /**< One PID controller per motor */

    /* ===== Sensors ===== */
    /* TODO: Re-enable once DS18B20 and 1-Wire are implemented */
    /* star_ds18b20_handle_t temp_sensor; */
    bq7850_handle_t       bms;         /**< BQ7850 battery management system (I2C) */

    /* ===== Task Handles ===== */
    TaskHandle_t motor_task_handle;     /**< Motor control task handle */
    TaskHandle_t telemetry_task_handle; /**< Telemetry monitoring task handle */
    TaskHandle_t comm_task_handle;      /**< Communication task handle */

    /* ===== Synchronization ===== */
    SemaphoreHandle_t state_mutex; /**< Mutex protecting shared state variables */

    /* ===== Control Variables (per motor) ===== */
    float   setpoint_rpm[NUM_MOTORS];       /**< Target RPM for each motor */
    float   current_rpm[NUM_MOTORS];        /**< Measured RPM for each motor */
    int32_t encoder_counts[NUM_MOTORS];     /**< Accumulated encoder counts */
    int32_t encoder_hw_baseline[NUM_MOTORS]; /**< Hardware count baseline */

    /* ===== Monitoring Variables ===== */
    float motor_current_ma[NUM_MOTORS]; /**< Motor current in milliamps */

    /* ===== System State ===== */
    bool    system_enabled; /**< System enable flag (false = emergency stop) */
    uint8_t selected_motor; /**< Currently selected motor (0-3) via multiplexer */
} system_context_t;

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CONTEXT_H */
