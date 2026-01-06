/**
 * @file rx_obstacle_detect.c
 * @brief Basic obstacle detection and emergency stop using HC-SR04 sensors.
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_obstacle_detect.h"
#include "tx_api.h"
#include "rx_motor.h"
#include "rx_log.h"

#define OBSTACLE_DETECT_TASK_STACK_SIZE 2048
#define OBSTACLE_DETECT_TASK_PRIORITY   10

static TX_THREAD                       s_obstacle_detect_thread;
static UCHAR                           s_obstacle_detect_stack[OBSTACLE_DETECT_TASK_STACK_SIZE];
static bool                            s_initialized = false;
static bool                            s_running = false;
static bool                            s_obstacle_present = false;
static rx_obstacle_detect_config_t     s_config;
static uint8_t                         s_consecutive_detections[RX_OBSTACLE_DETECT_MAX_SENSORS];

// Forward declaration
static void obstacle_detect_task_entry(ULONG thread_input);

rx_err_t rx_obstacle_detect_init(const rx_obstacle_detect_config_t* config) {
    if (config == NULL || config->num_sensors == 0 || config->num_sensors > RX_OBSTACLE_DETECT_MAX_SENSORS || config->num_motors == 0) {
        return k_rx_err_invalid_argument;
    }

    s_config = *config;
    for (uint8_t i = 0; i < s_config.num_sensors; ++i) {
        if (s_config.sensors[i] == NULL) {
            return k_rx_err_invalid_argument;
        }
        s_consecutive_detections[i] = 0;
    }
    for (uint8_t i = 0; i < s_config.num_motors; ++i) {
        if (s_config.motors[i] == NULL) {
            return k_rx_err_invalid_argument;
        }
    }

    UINT tx_status = tx_thread_create(&s_obstacle_detect_thread,
                                      "ObstacleDetect",
                                      obstacle_detect_task_entry,
                                      0,
                                      s_obstacle_detect_stack,
                                      OBSTACLE_DETECT_TASK_STACK_SIZE,
                                      OBSTACLE_DETECT_TASK_PRIORITY,
                                      OBSTACLE_DETECT_TASK_PRIORITY,
                                      TX_NO_TIME_SLICE,
                                      TX_DONT_START);
    if (tx_status != TX_SUCCESS) {
        return k_rx_err_os_failure;
    }

    s_initialized = true;
    return k_rx_ok;
}

rx_err_t rx_obstacle_detect_start(void) {
    if (!s_initialized) {
        return k_rx_err_not_initialized;
    }
    if (s_running) {
        return k_rx_ok; // Already running
    }
    s_running = true;
    UINT tx_status = tx_thread_resume(&s_obstacle_detect_thread);
    return (tx_status == TX_SUCCESS) ? k_rx_ok : k_rx_err_os_failure;
}

rx_err_t rx_obstacle_detect_stop(void) {
    if (!s_initialized || !s_running) {
        return k_rx_ok;
    }
    s_running = false;
    UINT tx_status = tx_thread_suspend(&s_obstacle_detect_thread);
    return (tx_status == TX_SUCCESS) ? k_rx_ok : k_rx_err_os_failure;
}

bool rx_obstacle_detect_is_obstacle_present(void) {
    return s_obstacle_present;
}

static void internal_stop_all_motors(void) {
    for (uint8_t i = 0; i < s_config.num_motors; ++i) {
        rx_motor_stop(s_config.motors[i], true); // true for brake
    }
}

static void obstacle_detect_task_entry(ULONG thread_input) {
    (void)thread_input;

    while (1) {
        if (!s_running) {
            tx_thread_suspend(&s_obstacle_detect_thread);
            continue;
        }

        bool an_obstacle_was_detected = false;
        for (uint8_t i = 0; i < s_config.num_sensors; ++i) {
            float distance_cm = 0.0f;
            rx_err_t err = rx_hcsr04_measure_blocking(s_config.sensors[i], &distance_cm);

            if (err == k_rx_ok && distance_cm < s_config.detection_threshold_cm) {
                s_consecutive_detections[i]++;
                if (s_consecutive_detections[i] >= s_config.debounce_samples) {
                    an_obstacle_was_detected = true;
                    if (!s_obstacle_present) {
                        s_obstacle_present = true;
                        rx_log_warn("Obstacle", "Obstacle detected. Stopping motors.");
                        rx_log_warn_val("Obstacle", "Sensor index", i);
                        rx_log_warn_val("Obstacle", "Distance (cm)", (int32_t)distance_cm);
                        internal_stop_all_motors();
                        if (s_config.callback) {
                            s_config.callback(i, distance_cm, s_config.user_data);
                        }
                    }
                }
            } else {
                s_consecutive_detections[i] = 0;
            }
        }
        
        if (!an_obstacle_was_detected && s_obstacle_present) {
            s_obstacle_present = false;
            rx_log_info("Obstacle", "Obstacle cleared.");
            // Motors remain stopped until a manual override.
        }

        tx_thread_sleep(s_config.poll_interval_ms / 10); // Convert ms to ticks (assuming 10ms/tick)
    }
}
