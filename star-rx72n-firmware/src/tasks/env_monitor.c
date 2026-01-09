/* src/tasks/env_monitor.c */

/**
 * @file env_monitor.c
 * @brief Environment Monitor thread implementation
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tasks/env_monitor.h"
#include "hardware_pinout.h"
#include "motor_config.h"
#include "shared_state.h"
#include "star_hcsr04_config.h"
#include "rx_bus_manager.h"
#include "rx_ds18b20.h"
#include "rx_hcsr04.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_obstacle_detect.h"
#include <string.h>

static char s_tag[] = "env_mon";

static TX_THREAD s_env_monitor_thread;
static uint8_t   s_env_monitor_stack[k_stack_env_monitor];

/* HC-SR04 ultrasonic sensor handles (4 sensors) */
static rx_hcsr04_t s_hcsr04_sensors[k_hcsr04_count];

/* DS18B20 temperature sensor handle */
static rx_ds18b20_handle_t s_ds18b20_handle;

/* Bus manager for 1-Wire communication */
static rx_bus_manager_t s_bus_manager;

static void     env_monitor_entry(ULONG input);
static rx_err_t init_sensors(void);
static rx_err_t scan_obstacles(void);
static rx_err_t monitor_temperature(void);

UINT env_monitor_create(void)
{
    UINT status = tx_thread_create(&s_env_monitor_thread, s_tag, env_monitor_entry, 0,
                                    s_env_monitor_stack, k_stack_env_monitor,
                                    k_priority_env_monitor, k_priority_env_monitor,
                                    TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Thread creation failed");
        return status;
    }

    rx_log_info(s_tag, "Env_Monitor thread created (priority 5, 50 Hz)");
    return TX_SUCCESS;
}

static void env_monitor_entry(ULONG input)
{
    (void)input;

    rx_log_info(s_tag, "Env_Monitor thread started");

    /* Register with watchdog for task-level monitoring
     * Timeout = 3x period = 3 * 20ms = 60ms */
    rx_err_t ret = rx_iwdt_register_task("Env_Monitor", 60);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Failed to register with watchdog");
    }

    /* Initialize sensors */
    ret = init_sensors();
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Sensor initialization failed");
        /* Cannot continue - halt this thread */
        while (1) {
            tx_thread_sleep(100);
        }
    }

    rx_log_info(s_tag, "Sensors initialized, entering main loop");

    /* Main monitoring loop (50 Hz = 20ms period) */
    while (1) {
        scan_obstacles();
        monitor_temperature();

        /* Record task heartbeat for deadlock detection */
        rx_iwdt_task_heartbeat("Env_Monitor");

        tx_thread_sleep(2); /* 2 ticks = 20ms at 100Hz ThreadX tick */
    }
}

static rx_err_t init_sensors(void)
{
    rx_err_t ret;

    /* -------------------------------------------------------------------------
     * Initialize HC-SR04 Ultrasonic Sensors (4 sensors)
     * -------------------------------------------------------------------------
     * Front, left, right, back obstacle detection
     */
    rx_log_info(s_tag, "Initializing HC-SR04 sensors");

    for (uint8_t i = 0; i < k_star_hcsr04_sensor_count; i++) {
        rx_hcsr04_config_t sensor_config;
        ret = star_hcsr04_get_config((star_hcsr04_sensor_id_t)i, &sensor_config);
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "Failed to get HC-SR04 config");
            return ret;
        }

        ret = rx_hcsr04_init(&s_hcsr04_sensors[i], &sensor_config);
        if (ret != k_rx_ok) {
            rx_log_error(s_tag, "HC-SR04 init failed");
            return ret;
        }
    }

    rx_log_info(s_tag, "HC-SR04 sensors initialized");

    /* -------------------------------------------------------------------------
     * Initialize DS18B20 Temperature Sensor (1-Wire)
     * -------------------------------------------------------------------------
     * On P05 (pin 100) with 4.7kΩ pull-up to 3.3V
     * Uses bus_manager pattern for hardware abstraction
     */
    rx_log_info(s_tag, "Initializing DS18B20 temperature sensor");

    /* Initialize bus manager for 1-Wire */
    ret = rx_bus_manager_init(&s_bus_manager, s_tag, NULL, NULL);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Bus manager init failed");
        return ret;
    }

    /* Create bus configuration node with 1-Wire config in proto union */
    static rx_bus_config_t bus_config = {
        .name        = "onewire0",
        .type        = k_bus_type_onewire,
        .initialized = false,
        .handle      = NULL,
        .user_ctx    = NULL,
        .proto.onewire.pin = k_gpio_p05, /* P05 (pin 100) */
        .next        = NULL,
    };

    /* Register 1-Wire bus with manager */
    ret = rx_bus_manager_add_bus(&s_bus_manager, &bus_config);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Failed to register 1-Wire bus");
        return ret;
    }

    /* Configure DS18B20 to use bus manager */
    rx_ds18b20_config_t temp_config = {
        .bus_manager      = &s_bus_manager,
        .bus_name         = "onewire0",
        .resolution       = k_ds18b20_resolution_12bit,
        .use_rom_matching = false, /* Single device mode (skip ROM) */
    };

    ret = rx_ds18b20_init(&s_ds18b20_handle, &temp_config);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "DS18B20 init failed");
        return ret;
    }

    rx_log_info(s_tag, "DS18B20 sensor initialized");

    return k_rx_ok;
}

static rx_err_t scan_obstacles(void)
{
    /* -------------------------------------------------------------------------
     * Issue 14: Obstacle Detection Integration
     * -------------------------------------------------------------------------
     * Scan all 4 HC-SR04 ultrasonic sensors
     * Check if any obstacle is within threshold (30cm)
     * Update safety state and trigger E-STOP if detected
     */

    shared_state_t* state = shared_state_get();
    bool obstacle_detected = false;
    float min_distance = k_obstacle_threshold_cm;
    uint8_t detecting_sensor = 0;

    for (uint8_t i = 0; i < k_star_hcsr04_sensor_count; i++) {
        float distance_cm = 0.0f;
        rx_err_t ret = rx_hcsr04_measure_blocking(&s_hcsr04_sensors[i], &distance_cm);

        if (ret != k_rx_ok) {
            /* Sensor read failed - continue to next sensor */
            continue;
        }

        /* Check if obstacle is within threshold */
        if (distance_cm < k_obstacle_threshold_cm) {
            obstacle_detected = true;
            if (distance_cm < min_distance) {
                min_distance = distance_cm;
                detecting_sensor = i;
            }
        }
    }

    /* Update safety state (mutex-protected) */
    UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire safety mutex");
        return k_rx_err_threadx;
    }

    const bool previous_obstacle = state->safety.obstacle_detected;

    state->safety.obstacle_detected = obstacle_detected;
    state->safety.obstacle_distance_cm = min_distance;
    state->safety.obstacle_sensor_idx = detecting_sensor;

    if (obstacle_detected && !previous_obstacle) {
        /* New obstacle detected - trigger E-STOP */
        rx_log_error(s_tag, "Obstacle detected - E-STOP triggered");
        state->safety.emergency_stop = true;
    } else if (!obstacle_detected && previous_obstacle) {
        /* Obstacle cleared */
        rx_log_info(s_tag, "Obstacle cleared");
        /* Note: E-STOP flag remains set until manual clearance */
    }

    tx_mutex_put(&state->safety_mutex);

    return k_rx_ok;
}

static rx_err_t monitor_temperature(void)
{
    /* -------------------------------------------------------------------------
     * Issue 15: Temperature Monitoring Integration
     * -------------------------------------------------------------------------
     * Read DS18B20 temperature sensor (1-Wire)
     * Update health state
     * Trigger E-STOP if temperature > 85°C (thermal protection)
     */

    shared_state_t* state = shared_state_get();

    /* Trigger conversion and read temperature */
    rx_err_t ret = rx_ds18b20_trigger_conversion(&s_ds18b20_handle);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Temperature conversion failed");
        return ret;
    }

    /* Wait for conversion to complete (~750ms for 12-bit resolution) */
    tx_thread_sleep(8);  /* 80ms should be enough for 12-bit */

    float temperature_c = 0.0f;
    ret = rx_ds18b20_read_temperature(&s_ds18b20_handle, &temperature_c);
    if (ret != k_rx_ok) {
        rx_log_error(s_tag, "Temperature read failed");
        return ret;
    }

    /* Update health state (mutex-protected) */
    UINT status = tx_mutex_get(&state->health_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS) {
        rx_log_error(s_tag, "Failed to acquire health mutex");
        return k_rx_err_threadx;
    }

    state->health.temperature_c = temperature_c;

    /* Check for thermal warning */
    if (temperature_c > k_temp_warning_threshold_c) {
        state->health.thermal_warning = true;
        if (temperature_c > k_temp_shutdown_threshold_c) {
            /* Thermal shutdown - trigger E-STOP */
            rx_log_error(s_tag, "Thermal shutdown - E-STOP triggered");

            status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
            if (status == TX_SUCCESS) {
                state->safety.emergency_stop = true;
                tx_mutex_put(&state->safety_mutex);
            }
        } else {
            rx_log_error(s_tag, "Thermal warning");
        }
    } else {
        state->health.thermal_warning = false;
    }

    tx_mutex_put(&state->health_mutex);

    return k_rx_ok;
}
