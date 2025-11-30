#ifndef DHT22_TASK_H
#define DHT22_TASK_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "star_bus_manager.h"

/*
 * DHT22 Temperature/Humidity Monitoring Task
 * Handles DHT22 sensor readings in a dedicated FreeRTOS task
 * Provides periodic temperature updates for sound speed correction
 */

typedef struct {
  star_bus_manager_t* bus_manager;
  const char*         bus_name;
  TaskHandle_t        task_handle;
  bool                task_running;
  bool                sensor_initialized;
  float               last_valid_temperature;
  uint32_t            consecutive_failures;
} dht22_task_context_t;

/**
 * @brief Initialize DHT22 monitoring task
 * 
 * Creates and starts the FreeRTOS task for temperature/humidity monitoring.
 * The task will periodically read the DHT22 sensor and publish data
 * to the shared data queues.
 * 
 * @param bus_manager Initialized bus manager
 * @param bus_name Name of bus for DHT22 sensor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t dht22_task_start(star_bus_manager_t* bus_manager, const char* bus_name);

/**
 * @brief Stop DHT22 monitoring task
 * 
 * Cleanly shuts down the DHT22 task and deinitializes the sensor.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t dht22_task_stop(void);

/**
 * @brief Check if DHT22 task is running
 * 
 * @return true if task is active and monitoring sensor
 */
bool dht22_task_is_running(void);

/**
 * @brief Get last valid temperature reading
 * 
 * Returns the most recent successful temperature reading from DHT22.
 * Useful for fallback when current readings fail.
 * 
 * @param temperature_c Pointer to store temperature value
 * @return true if a valid temperature is available
 */
bool dht22_task_get_last_temperature(float* temperature_c);

/**
 * @brief Get DHT22 task health statistics
 * 
 * Returns information about task performance and sensor reliability.
 * 
 * @param consecutive_failures Number of consecutive read failures
 * @param sensor_initialized True if sensor was successfully initialized
 * @return true if statistics are available
 */
bool dht22_task_get_health(uint32_t* consecutive_failures, bool* sensor_initialized);

#endif /* DHT22_TASK_H */