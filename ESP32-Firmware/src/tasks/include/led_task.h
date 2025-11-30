#ifndef LED_TASK_H
#define LED_TASK_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../../modules/include/led_controller.h"
#include "star_sensor_pca9685.h"

/*
 * LED Display Management Task
 * Handles LED updates based on sensor readings in a dedicated FreeRTOS task
 * Provides responsive visual feedback with smooth color transitions
 */

typedef struct {
  const pca9685_handle_t* pca_handle;
  led_controller_t        led_controller;
  TaskHandle_t            task_handle;
  bool                    task_running;
  bool                    leds_initialized;
  uint32_t                update_failures;
  uint32_t                successful_updates;
} led_task_context_t;

/**
 * @brief Initialize LED display task
 * 
 * Creates and starts the FreeRTOS task for LED management.
 * The task will monitor sensor data queues and update LEDs accordingly.
 * 
 * @param pca_handle Initialized PCA9685 handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_task_start(const pca9685_handle_t* pca_handle);

/**
 * @brief Stop LED display task
 * 
 * Cleanly shuts down the LED task and turns off all LEDs.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_task_stop(void);

/**
 * @brief Check if LED task is running
 * 
 * @return true if task is active and managing LEDs
 */
bool led_task_is_running(void);

/**
 * @brief Get LED task performance statistics
 * 
 * Returns information about LED update performance and reliability.
 * 
 * @param update_failures Number of failed LED updates
 * @param successful_updates Number of successful LED updates
 * @return true if statistics are available
 */
bool led_task_get_stats(uint32_t* update_failures, uint32_t* successful_updates);

/**
 * @brief Manually trigger LED update for specific sensor
 * 
 * Forces an immediate LED update for the specified sensor index.
 * Useful for testing or emergency signaling.
 * 
 * @param sensor_index Sensor index (0 = left, 1 = right)
 * @param distance_cm Distance to display (or negative for LED off)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_task_manual_update(uint8_t sensor_index, float distance_cm);

/**
 * @brief Turn off all LEDs immediately
 * 
 * Emergency function to turn off all LEDs regardless of sensor readings.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_task_emergency_off(void);

#endif /* LED_TASK_H */