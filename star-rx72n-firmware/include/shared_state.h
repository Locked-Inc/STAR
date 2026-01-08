/* include/shared_state.h */

/**
 * @file shared_state.h
 * @brief Shared state module for inter-thread communication
 * @details
 * Provides mutex-protected shared state structures for safe inter-thread
 * communication in the STAR RX72N motor control firmware.
 *
 * Architecture:
 * - Motor setpoint: Commands from Comm_Manager → Motor_Controller
 * - Encoder feedback: Data from Motor_Controller → Comm_Manager
 * - Safety state: Emergency stop flags from multiple sources → Motor_Controller
 * - System health: Battery/temp data from Env_Monitor/System_Health → Comm_Manager
 *
 * Thread Safety:
 * All structures are protected by separate mutexes to prevent deadlocks.
 * Each thread should acquire only one mutex at a time.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include "tx_api.h"
#include "motor_config.h"
#include <stdbool.h>
#include <stdint.h>

/* =============================================================================
 * Motor Setpoint (Comm_Manager → Motor_Controller)
 * =============================================================================
 */

/**
 * @brief Motor velocity setpoint
 * @details Commands from RPi5 via USB/SPI to Motor_Controller
 */
typedef struct {
    float    velocity_mps[k_motor_count]; /**< Target velocity (m/s) for each motor */
    uint32_t sequence;                    /**< Command sequence number */
    uint32_t timestamp_ms;                /**< Timestamp when command received */
    bool     valid;                       /**< True if setpoint is valid */
} motor_setpoint_t;

/* =============================================================================
 * Encoder Feedback (Motor_Controller → Comm_Manager)
 * =============================================================================
 */

/**
 * @brief Encoder feedback data
 * @details Measured encoder position and velocity for telemetry
 */
typedef struct {
    int32_t  ticks;       /**< Encoder ticks (quadrature counts) */
    float    velocity_mps; /**< Measured velocity (m/s) */
    uint32_t timestamp_ms; /**< Timestamp when data captured */
} encoder_data_t;

/**
 * @brief Encoder feedback for all motors
 */
typedef struct {
    encoder_data_t motors[k_motor_count]; /**< Feedback for each motor */
} encoder_feedback_t;

/* =============================================================================
 * Safety State (Multiple Sources → Motor_Controller)
 * =============================================================================
 */

/**
 * @brief Fault flags bitfield
 * @details Each bit represents a fault for one motor (bits 0-3)
 */
typedef enum {
    k_fault_motor_0 = (1 << 0), /**< Motor 0 fault (overcurrent, thermal, etc.) */
    k_fault_motor_1 = (1 << 1), /**< Motor 1 fault */
    k_fault_motor_2 = (1 << 2), /**< Motor 2 fault */
    k_fault_motor_3 = (1 << 3), /**< Motor 3 fault */
} fault_flags_t;

/**
 * @brief Safety state
 * @details Emergency stop and fault tracking
 *
 * E-STOP Sources:
 * - Env_Monitor: Obstacle detection (HC-SR04 < 30cm)
 * - Comm_Manager: Communication timeout (500ms)
 * - Motor_Controller: Motor faults (nFAULT pins)
 * - Env_Monitor: Thermal protection (DS18B20 > 85°C)
 *
 * Clearance:
 * - Manual clearance required via ClearEmergencyStop command
 * - Only allowed if no active obstacles, faults, or timeouts
 */
typedef struct {
    bool     emergency_stop;   /**< Master E-STOP flag (checked every 4ms) */
    bool     obstacle_detected; /**< Obstacle within threshold distance */
    bool     comm_timeout;     /**< Communication timeout (500ms) */
    uint8_t  fault_flags;      /**< Motor fault bitfield (fault_flags_t) */
    float    obstacle_distance_cm; /**< Distance to nearest obstacle */
    uint8_t  obstacle_sensor_idx;  /**< Index of sensor detecting obstacle */
} safety_state_t;

/* =============================================================================
 * System Health (Env_Monitor/System_Health → Comm_Manager)
 * =============================================================================
 */

/**
 * @brief System health data
 * @details Battery, temperature, and diagnostics
 */
typedef struct {
    float    battery_voltage_v;  /**< Battery voltage (V) */
    float    battery_soc_percent; /**< State of charge (%) */
    float    battery_current_ma;  /**< Battery current (mA) */
    float    temperature_c;       /**< System temperature (°C) */
    bool     battery_low;         /**< True if SOC < 20% */
    bool     thermal_warning;     /**< True if temp > 70°C */
    uint32_t uptime_ms;           /**< System uptime (ms) */
} system_health_t;

/* =============================================================================
 * Shared State Container
 * =============================================================================
 */

/**
 * @brief Shared state container with mutex protection
 * @details All inter-thread communication goes through this structure
 *
 * Mutex Acquisition Rules:
 * - Never hold multiple mutexes simultaneously (prevents deadlock)
 * - Acquire mutex, read/write data, release immediately
 * - Use TX_WAIT_FOREVER for mutex acquisition (critical data)
 */
typedef struct {
    motor_setpoint_t    setpoint; /**< Current motor commands */
    encoder_feedback_t  encoders; /**< Encoder data (4 motors) */
    safety_state_t      safety;   /**< E-STOP, obstacles, faults */
    system_health_t     health;   /**< Battery, temperature */

    TX_MUTEX setpoint_mutex; /**< Protects setpoint */
    TX_MUTEX encoder_mutex;  /**< Protects encoders */
    TX_MUTEX safety_mutex;   /**< Protects safety */
    TX_MUTEX health_mutex;   /**< Protects health */
} shared_state_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize shared state module
 * @details Creates all mutexes and initializes state structures
 *
 * Must be called in tx_application_define() before creating any tasks.
 *
 * @param state Pointer to shared state structure
 * @return TX_SUCCESS on success, error code otherwise
 */
UINT shared_state_init(shared_state_t* state);

/**
 * @brief Get global shared state instance
 * @details Returns pointer to the global shared state structure
 *
 * @return Pointer to shared state (never NULL after shared_state_init())
 */
shared_state_t* shared_state_get(void);

#endif /* SHARED_STATE_H */
