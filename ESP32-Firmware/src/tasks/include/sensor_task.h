#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../../include/system_config.h"
#include "star_bus_manager.h"
#include "star_error_interface.h"
#include "star_sensor_hcsr04.h"

/*
 * Sensor Management Task
 * Handles HC-SR04 ultrasonic sensors in a dedicated FreeRTOS task
 * Provides concurrent, non-blocking distance measurements
 */

typedef struct {
  star_bus_manager_t*     bus_manager;
  star_error_interface_t* error_iface;
  const char*             bus_name;
  TaskHandle_t            task_handle;
  hcsr04_handle_t sensor_handles[STAR_SYSTEM_HCSR04_SENSOR_COUNT]; /* HC-SR04 sensor handles */
  bool            task_running;
  bool            sensors_initialized;
} sensor_task_context_t;

/**
 * @brief Initialize sensor management task
 * 
 * Creates and starts the FreeRTOS task for sensor monitoring.
 * The task will continuously read HC-SR04 sensors and publish data
 * to the shared data queues.
 * 
 * @param bus_manager Initialized bus manager
 * @param error_iface Error interface for retry handling
 * @param bus_name Name of GPIO bus for HC-SR04 sensors
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensor_task_start(star_bus_manager_t*     bus_manager,
                            star_error_interface_t* error_iface,
                            const char* const       bus_name);

/**
 * @brief Stop sensor management task
 * 
 * Cleanly shuts down the sensor task and deinitializes all sensors.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensor_task_stop(void);

/**
 * @brief Check if sensor task is running
 * 
 * @return true if task is active and monitoring sensors
 */
bool sensor_task_is_running(void);

/**
 * @brief Update temperature for sound speed correction
 * 
 * Updates the temperature used for calculating sound speed in
 * ultrasonic distance measurements.
 * 
 * @param temperature_c Temperature in Celsius
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensor_task_update_temperature(const float temperature_c);

#endif /* SENSOR_TASK_H */