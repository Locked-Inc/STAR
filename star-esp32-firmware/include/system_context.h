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
#include "star_config.h"
#include "star_encoder.h"
#include "star_error_handler.h"
#include "star_motor.h"
#include "star_pid.h"
#include "star_sensor_ds18b20.h"

#include "system_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System state machine states
 */
typedef enum {
    SYSTEM_STATE_INIT,          /**< System initializing */
    SYSTEM_STATE_ARMED,         /**< Ready to run, waiting for first command */
    SYSTEM_STATE_RUNNING,       /**< Normal operation */
    SYSTEM_STATE_DEGRADED,      /**< Running with reduced capability */
    SYSTEM_STATE_FAULT,         /**< Error detected, attempting recovery */
    SYSTEM_STATE_SAFE_SHUTDOWN, /**< Controlled shutdown in progress */
    SYSTEM_STATE_EMERGENCY_STOP /**< Emergency stop activated */
} system_state_t;

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
    star_config_t          config;        /**< Runtime configuration (NVS-backed) */

    /* ===== Motor Drivers ===== */
    star_motor_handle_t motors[NUM_MOTORS]; /**< Low-level MCPWM motor controllers */

    /* ===== Encoder (Multiplexed) ===== */
    star_encoder_handle_t encoder; /**< Single shared encoder (multiplexed across 4 motors) */

    /* ===== PID Controllers ===== */
    star_pid_handle_t pid[NUM_MOTORS]; /**< One PID controller per motor */

    /* ===== Sensors ===== */
    star_ds18b20_handle_t temp_sensor; /**< DS18B20 temperature sensor (1-Wire) */
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
    system_state_t current_state;  /**< Current system state */
    bool    system_enabled;        /**< System enable flag (false = emergency stop) */
    uint8_t selected_motor;        /**< Currently selected motor (0-3) via multiplexer */

    /* ===== Degradation Tracking ===== */
    bool encoder_failed;           /**< Encoder failure flag */
    bool bms_failed;               /**< BMS communication failed */
    bool temp_sensor_failed;       /**< Temperature sensor failed */
    uint32_t fault_count;          /**< Number of faults since boot */

    /* ===== Performance Monitoring ===== */
    uint32_t motor_task_max_time_us;    /**< Max motor task execution time (microseconds) */
    uint32_t telemetry_task_max_time_us; /**< Max telemetry task time */
    uint32_t comm_task_max_time_us;      /**< Max communication task time */
} system_context_t;

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CONTEXT_H */
